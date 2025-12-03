#pragma once
#include <iostream>
using namespace std;

class Array
{
private:
    struct Storage {
        int* data;
        int  size;
        int  refCount;
    };

    Storage* storage;
    int index;          // 当前 view 的起始线性下标
    int shape[16];      // 每一维大小
    int axisNum;        // 维度数
    int nowAxis;        // 已经通过 operator[] 索引到第几维

    int stride(int axis) const {
        if (axis >= axisNum) return 1;
        int s = 1;
        for (int i = axis + 1; i < axisNum; ++i)
            s *= shape[i];
        return s;
    }

    int total_size() const { return storage ? storage->size : 0; }

    void reshape_internal(int nAxis, const int* newShape) {
        int newSize = 1;
        for (int i = 0; i < nAxis; ++i)
            newSize *= newShape[i];
        if (!storage) return;
        if (newSize != storage->size) {
            // 元素总数不匹配，不做 reshape（也可以选择抛错）
            return;
        }
        axisNum = nAxis;
        for (int i = 0; i < nAxis; ++i)
            shape[i] = newShape[i];
        nowAxis = 0;
        index = 0;
    }

public:
    Array() {
        storage = nullptr;
        index = 0;
        axisNum = 0;
        nowAxis = 0;
        for (int i = 0; i < 16; ++i) shape[i] = 0;
    }

    // 构造：Array(3,3) / Array(4,4,4) / Array(16) 等
    template<typename... Args>
    Array(Args... args) {
        size_t list[] = { (size_t)args... };
        int nAxis = (int)sizeof...(args);

        axisNum = nAxis;
        nowAxis = 0;
        index = 0;
        int total = 1;
        for (int i = 0; i < nAxis; ++i) {
            shape[i] = (int)list[i];
            total *= shape[i];
        }
        for (int i = nAxis; i < 16; ++i) shape[i] = 0;

        storage = new Storage;
        storage->size = total;
        storage->refCount = 1;
        storage->data = new int[total];
        // 如需初始化为 0，可取消注释
        // for (int i = 0; i < total; ++i) storage->data[i] = 0;
    }

    // 拷贝构造：共享底层数据，引用计数+1
    Array(const Array& other) {
        storage = other.storage;
        index = other.index;
        axisNum = other.axisNum;
        nowAxis = other.nowAxis;
        for (int i = 0; i < 16; ++i) shape[i] = other.shape[i];
        if (storage) storage->refCount++;
    }

    ~Array() {
        if (storage) {
            storage->refCount--;
            if (storage->refCount == 0) {
                delete[] storage->data;
                delete storage;
            }
        }
    }

    // 赋值：深拷贝（c = a 之后各自独立）
    Array& operator=(const Array& other) {
        if (this == &other) return *this;

        if (storage) {
            storage->refCount--;
            if (storage->refCount == 0) {
                delete[] storage->data;
                delete storage;
            }
        }

        storage = nullptr;
        index = other.index;
        axisNum = other.axisNum;
        nowAxis = other.nowAxis;
        for (int i = 0; i < 16; ++i) shape[i] = other.shape[i];

        if (other.storage) {
            storage = new Storage;
            storage->size = other.storage->size;
            storage->refCount = 1;
            storage->data = new int[storage->size];
            for (int i = 0; i < storage->size; ++i)
                storage->data[i] = other.storage->data[i];
        }
        return *this;
    }

    // at(i,j,k,...)：返回指向该元素的一个视图
    template<typename... Args>
    Array at(Args... args) {
        size_t idxList[] = { (size_t)args... };
        int nIdx = (int)sizeof...(args);

        Array sub(*this);
        int flat = index;
        for (int d = 0; d < nIdx; ++d) {
            int s = 1;
            for (int j = d + 1; j < axisNum; ++j)
                s *= shape[j];
            flat += (int)idxList[d] * s;
        }
        sub.index = flat;
        sub.nowAxis = axisNum;
        return sub;
    }

    // reshape(4,4) 等
    template<typename... Args>
    void reshape(Args... args) {
        size_t list[] = { (size_t)args... };
        int nAxis = (int)sizeof...(args);
        int tmp[16];
        for (int i = 0; i < nAxis; ++i)
            tmp[i] = (int)list[i];
        reshape_internal(nAxis, tmp);
    }

    // 获取 C 风格数据指针
    int* get_content() {
        return storage ? storage->data : nullptr;
    }

    // 整个数组全部设为某个值
    void set(int value) {
        if (!storage) return;
        for (int i = 0; i < storage->size; ++i)
            storage->data[i] = value;
    }

    // 支持 a[i][j][k] 链式索引
    Array operator[](int idx) const {
        Array sub(*this);
        int s = stride(nowAxis);
        sub.index = index + idx * s;
        sub.nowAxis = nowAxis + 1;
        return sub;
    }

    // a[i][j] = 5;
    Array& operator=(int value) {
        if (storage && storage->data)
            storage->data[index] = value;
        return *this;
    }

    // int x = a[i][j];
    operator int() const {
        if (storage && storage->data)
            return storage->data[index];
        return 0;
    }

    // —— 下方是元素级运算（加减乘除），可按作业需要使用 —— //

    Array operator+(const Array& other) const {
        Array result(total_size());
        result.reshape_internal(axisNum, shape);
        int n = total_size();
        int* rd = result.storage->data;
        int* a = storage->data;
        int* b = other.storage->data;
        for (int i = 0; i < n; ++i) rd[i] = a[i] + b[i];
        return result;
    }

    Array operator+(int s) const {
        Array result(total_size());
        result.reshape_internal(axisNum, shape);
        int n = total_size();
        int* rd = result.storage->data;
        int* a = storage->data;
        for (int i = 0; i < n; ++i) rd[i] = a[i] + s;
        return result;
    }

    Array operator-(const Array& other) const {
        Array result(total_size());
        result.reshape_internal(axisNum, shape);
        int n = total_size();
        int* rd = result.storage->data;
        int* a = storage->data;
        int* b = other.storage->data;
        for (int i = 0; i < n; ++i) rd[i] = a[i] - b[i];
        return result;
    }

    Array operator-(int s) const {
        Array result(total_size());
        result.reshape_internal(axisNum, shape);
        int n = total_size();
        int* rd = result.storage->data;
        int* a = storage->data;
        for (int i = 0; i < n; ++i) rd[i] = a[i] - s;
        return result;
    }

    Array operator*(const Array& other) const {
        Array result(total_size());
        result.reshape_internal(axisNum, shape);
        int n = total_size();
        int* rd = result.storage->data;
        int* a = storage->data;
        int* b = other.storage->data;
        for (int i = 0; i < n; ++i) rd[i] = a[i] * b[i];
        return result;
    }

    Array operator*(int s) const {
        Array result(total_size());
        result.reshape_internal(axisNum, shape);
        int n = total_size();
        int* rd = result.storage->data;
        int* a = storage->data;
        for (int i = 0; i < n; ++i) rd[i] = a[i] * s;
        return result;
    }

    Array operator/(const Array& other) const {
        Array result(total_size());
        result.reshape_internal(axisNum, shape);
        int n = total_size();
        int* rd = result.storage->data;
        int* a = storage->data;
        int* b = other.storage->data;
        for (int i = 0; i < n; ++i) rd[i] = a[i] / b[i]; // 默认不传入除 0
        return result;
    }

    Array operator/(int s) const {
        Array result(total_size());
        result.reshape_internal(axisNum, shape);
        int n = total_size();
        int* rd = result.storage->data;
        int* a = storage->data;
        for (int i = 0; i < n; ++i) rd[i] = a[i] / s;
        return result;
    }

    friend Array operator+(int s, const Array& arr) {
        return arr + s;
    }
    friend Array operator-(int s, const Array& arr) {
        Array result(arr.total_size());
        result.reshape_internal(arr.axisNum, arr.shape);
        int n = arr.total_size();
        int* rd = result.storage->data;
        int* a = arr.storage->data;
        for (int i = 0; i < n; ++i) rd[i] = s - a[i];
        return result;
    }
};
