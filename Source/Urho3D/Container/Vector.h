//
// Copyright (c) 2008-2016 the Urho3D project.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#pragma once

#include "../Container/VectorBase.h"

#include <cassert>
#include <cstring>
#include <new>
#if URHO3D_CXX11
#include <initializer_list>
#endif

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:6293)
#endif

namespace Urho3D
{

/// %Vector template class.
template <class T> class Vector : public VectorBase
{
public:
    typedef T ValueType;
    typedef RandomAccessIterator<T> Iterator;
    typedef RandomAccessConstIterator<T> ConstIterator;

    /// Construct empty.
    Vector()
    {
    }

    /// Construct with initial size.
    explicit Vector(unsigned size)
    {
        Resize(size, (const T*)0);
    }

    /// Construct with initial data.
    Vector(const T* data, unsigned size)
    {
        Resize(size, data);
    }

    /// Construct from another vector.
    Vector(const Vector<T>& vector)
    {
        *this = vector;
    }
#if URHO3D_CXX11
    /// Aggregate initialization constructor.
    Vector(const std::initializer_list<T>& list) : Vector()
    {
        for (auto it = list.begin(); it != list.end(); it++)
        {
            Push(*it);
        }
    }
#endif
    /// Destruct.
    ~Vector()
    {
        Clear();
        delete[] buffer_;
    }

    /// Assign from another vector.
    Vector<T>& operator =(const Vector<T>& rhs)
    {
        Clear();
        Resize(rhs.size_, rhs.Buffer());
        return *this;
    }

    /// Add-assign an element.
    Vector<T>& operator +=(const T& rhs)
    {
        Push(rhs);
        return *this;
    }

    /// Add-assign another vector.
    Vector<T>& operator +=(const Vector<T>& rhs)
    {
        Push(rhs);
        return *this;
    }

    /// Add an element.
    Vector<T> operator +(const T& rhs) const
    {
        Vector<T> ret(*this);
        ret.Push(rhs);
        return ret;
    }

    /// Add another vector.
    Vector<T> operator +(const Vector<T>& rhs) const
    {
        Vector<T> ret(*this);
        ret.Push(rhs);
        return ret;
    }

    /// Test for equality with another vector.
    bool operator ==(const Vector<T>& rhs) const
    {
        if (rhs.size_ != size_)
            return false;

        T* buffer = Buffer();
        T* rhsBuffer = rhs.Buffer();
        for (unsigned i = 0; i < size_; ++i)
        {
            if (buffer[i] != rhsBuffer[i])
                return false;
        }

        return true;
    }

    /// Test for inequality with another vector.
    bool operator !=(const Vector<T>& rhs) const
    {
        if (rhs.size_ != size_)
            return true;

        T* buffer = Buffer();
        T* rhsBuffer = rhs.Buffer();
        for (unsigned i = 0; i < size_; ++i)
        {
            if (buffer[i] != rhsBuffer[i])
                return true;
        }

        return false;
    }

    /// Return element at index.
    T& operator [](unsigned index)
    {
        assert(index < size_);
        return Buffer()[index];
    }

    /// Return const element at index.
    const T& operator [](unsigned index) const
    {
        assert(index < size_);
        return Buffer()[index];
    }

    /// Return element at index.
    T& At(unsigned index)
    {
        assert(index < size_);
        return Buffer()[index];
    }

    /// Return const element at index.
    const T& At(unsigned index) const
    {
        assert(index < size_);
        return Buffer()[index];
    }

    /// Add an element at the end.
#ifndef COVERITY_SCAN_MODEL
    void Push(const T& value)
	{
        if (size_ < capacity_)
        {
            // Optimize common case
            ++size_;
            new (&Back()) T(value);
        }
        else
			Resize(size_ + 1, &value);
	}
#else
    // FIXME: Attempt had been made to use this model in the Coverity-Scan model file without any success
    // Probably because the model had generated a different mangled name than the one used by static analyzer
    void Push(const T& value)
    {
        T array[] = {value};
        Resize(size_ + 1, array);
    }
#endif

    /// Add another vector at the end.
    void Push(const Vector<T>& vector) { Resize(size_ + vector.size_, vector.Buffer()); }

    /// Remove the last element.
    void Pop()
    {
        if (size_)
            Resize(size_ - 1, (const T*)0);
    }

    /// Insert an element at position.
    void Insert(unsigned pos, const T& value)
    {
        if (pos > size_)
            pos = size_;

        unsigned oldSize = size_;
        Resize(size_ + 1, (const T*)0);
        MoveRange(pos + 1, pos, oldSize - pos);
        Buffer()[pos] = value;
    }

    /// Insert another vector at position.
    void Insert(unsigned pos, const Vector<T>& vector)
    {
        if (pos > size_)
            pos = size_;

        unsigned oldSize = size_;
        Resize(size_ + vector.size_, (const T*)0);
        MoveRange(pos + vector.size_, pos, oldSize - pos);
        CopyElements(Buffer() + pos, vector.Buffer(), vector.size_);
    }

    /// Insert an element by iterator.
    Iterator Insert(const Iterator& dest, const T& value)
    {
        unsigned pos = (unsigned)(dest - Begin());
        if (pos > size_)
            pos = size_;
        Insert(pos, value);

        return Begin() + pos;
    }

    /// Insert a vector by iterator.
    Iterator Insert(const Iterator& dest, const Vector<T>& vector)
    {
        unsigned pos = (unsigned)(dest - Begin());
        if (pos > size_)
            pos = size_;
        Insert(pos, vector);

        return Begin() + pos;
    }

    /// Insert a vector partially by iterators.
    Iterator Insert(const Iterator& dest, const ConstIterator& start, const ConstIterator& end)
    {
        unsigned pos = (unsigned)(dest - Begin());
        if (pos > size_)
            pos = size_;
        unsigned length = (unsigned)(end - start);
        Resize(size_ + length, (const T*)0);
        MoveRange(pos + length, pos, size_ - pos - length);

        T* destPtr = Buffer() + pos;
        for (ConstIterator it = start; it != end; ++it)
            *destPtr++ = *it;

        return Begin() + pos;
    }

    /// Insert elements.
    Iterator Insert(const Iterator& dest, const T* start, const T* end)
    {
        unsigned pos = (unsigned)(dest - Begin());
        if (pos > size_)
            pos = size_;
        unsigned length = (unsigned)(end - start);
        Resize(size_ + length, (const T*)0);
        MoveRange(pos + length, pos, size_ - pos - length);

        T* destPtr = Buffer() + pos;
        for (const T* i = start; i != end; ++i)
            *destPtr++ = *i;

        return Begin() + pos;
    }

    /// Erase a range of elements.
    void Erase(unsigned pos, unsigned length = 1)
    {
        // Return if the range is illegal
        if (pos + length > size_ || !length)
            return;

        MoveRange(pos, pos + length, size_ - pos - length);
        Resize(size_ - length, (const T*)0);
    }

    /// Erase a range of elements by swapping elements from the end of the array.
    void EraseSwap(unsigned pos, unsigned length = 1)
    {
        unsigned shiftStartIndex = pos + length;
        // Return if the range is illegal
        if (shiftStartIndex > size_ || !length)
            return;

        unsigned newSize = size_ - length;
        unsigned trailingCount = size_ - shiftStartIndex;
        if (trailingCount <= length)
        {
            // We're removing more elements from the array than exist past the end of the range being removed, so
            // perform a normal shift and destroy.
            MoveRange(pos, shiftStartIndex, trailingCount);
        }
        else
        {
            // Swap elements from the end of the array into the empty space.
            CopyElements(Buffer() + pos, Buffer() + newSize, length);
        }
        Resize(newSize, (const T*)0);
    }

    /// Erase an element by iterator. Return iterator to the next element.
    Iterator Erase(const Iterator& it)
    {
        unsigned pos = (unsigned)(it - Begin());
        if (pos >= size_)
            return End();
        Erase(pos);

        return Begin() + pos;
    }

    /// Erase a range by iterators. Return iterator to the next element.
    Iterator Erase(const Iterator& start, const Iterator& end)
    {
        unsigned pos = (unsigned)(start - Begin());
        if (pos >= size_)
            return End();
        unsigned length = (unsigned)(end - start);
        Erase(pos, length);

        return Begin() + pos;
    }

    /// Erase an element by value. Return true if was found and erased.
    bool Remove(const T& value)
    {
        Iterator i = Find(value);
        if (i != End())
        {
            Erase(i);
            return true;
        }
        else
            return false;
    }

    /// Erase an element by value by swapping with the last element. Return true if was found and erased.
    bool RemoveSwap(const T& value)
    {
        Iterator i = Find(value);
        if (i != End())
        {
            EraseSwap(i - Begin());
            return true;
        }
        else
            return false;
    }

    /// Clear the vector.
    void Clear() { Resize(0); }

    /// Resize the vector.
    void Resize(unsigned newSize) { Resize(newSize, (const T*)0); }

    /// Set new capacity.
    void Reserve(unsigned newCapacity)
    {
        if (newCapacity < size_)
            newCapacity = size_;

        if (newCapacity != capacity_)
        {
            T* newBuffer = 0;
            capacity_ = newCapacity;

            if (capacity_)
            {
                newBuffer = reinterpret_cast<T*>(AllocateBuffer((unsigned)(capacity_ * sizeof(T))));
                // Move the data into the new buffer
                ConstructElements(newBuffer, Buffer(), size_);
            }

            // Delete the old buffer
            DestructElements(Buffer(), size_);
            delete[] buffer_;
            buffer_ = reinterpret_cast<unsigned char*>(newBuffer);
        }
    }

    /// Reallocate so that no extra memory is used.
    void Compact() { Reserve(size_); }

    /// Return iterator to value, or to the end if not found.
    Iterator Find(const T& value)
    {
        Iterator it = Begin();
        while (it != End() && *it != value)
            ++it;
        return it;
    }

    /// Return const iterator to value, or to the end if not found.
    ConstIterator Find(const T& value) const
    {
        ConstIterator it = Begin();
        while (it != End() && *it != value)
            ++it;
        return it;
    }

    /// Return whether contains a specific value.
    bool Contains(const T& value) const { return Find(value) != End(); }

    /// Return iterator to the beginning.
    Iterator Begin() { return Iterator(Buffer()); }

    /// Return const iterator to the beginning.
    ConstIterator Begin() const { return ConstIterator(Buffer()); }

    /// Return iterator to the end.
    Iterator End() { return Iterator(Buffer() + size_); }

    /// Return const iterator to the end.
    ConstIterator End() const { return ConstIterator(Buffer() + size_); }

    /// Return first element.
    T& Front()
    {
        assert(size_);
        return Buffer()[0];
    }

    /// Return const first element.
    const T& Front() const
    {
        assert(size_);
        return Buffer()[0];
    }

    /// Return last element.
    T& Back()
    {
        assert(size_);
        return Buffer()[size_ - 1];
    }

    /// Return const last element.
    const T& Back() const
    {
        assert(size_);
        return Buffer()[size_ - 1];
    }

    /// Return size of vector.
    unsigned Size() const { return size_; }

    /// Return capacity of vector.
    unsigned Capacity() const { return capacity_; }

    /// Return whether vector is empty.
    bool Empty() const { return size_ == 0; }

    /// Return the buffer with right type.
    T* Buffer() const { return reinterpret_cast<T*>(buffer_); }

	void Resize(unsigned newsize, const T& value) { Resize(newsize, &value); }

private:

    /// Copy-construct elements
    static void ConstructElements(T* dest, const T* src, unsigned count)
    {
        if (!src)
        {
            for (unsigned i = 0; i < count; ++i)
                new(dest + i) T();
        }
        else
        {
            for (unsigned i = 0; i < count; ++i)
                new(dest + i) T(*(src + i));
        }
    }

    /// Resize the vector and create/remove new elements as necessary.
    void Resize(unsigned newSize, const T* src)
    {
        // If size shrinks, destruct the removed elements
        if (newSize < size_)
            DestructElements(Buffer() + newSize, size_ - newSize);
        else
        {
            // Allocate new buffer if necessary and copy the current elements
            if (newSize > capacity_)
            {
                if (!capacity_)
                    capacity_ = newSize;
                else
                {
                    while (capacity_ < newSize)
                        capacity_ += (capacity_ + 1) >> 1;
                }

                unsigned char* newBuffer = AllocateBuffer((unsigned)(capacity_ * sizeof(T)));
                if (buffer_)
                {
                    ConstructElements(reinterpret_cast<T*>(newBuffer), Buffer(), size_);
                    DestructElements(Buffer(), size_);
                    delete[] buffer_;
                }
                buffer_ = newBuffer;
            }

            // Initialize the new elements
            ConstructElements(Buffer() + size_, src, newSize - size_);
        }

        size_ = newSize;
    }

    /// Move a range of elements within the vector.
    void MoveRange(unsigned dest, unsigned src, unsigned count)
    {
        T* buffer = Buffer();
        if (src < dest)
        {
            for (unsigned i = count - 1; i < count; --i)
                buffer[dest + i] = buffer[src + i];
        }
        if (src > dest)
        {
            for (unsigned i = 0; i < count; ++i)
                buffer[dest + i] = buffer[src + i];
        }
    }

    /// Copy elements from one buffer to another.
    static void CopyElements(T* dest, const T* src, unsigned count)
    {
        while (count--)
            *dest++ = *src++;
    }

    /// Call the elements' destructors.
    static void DestructElements(T* dest, unsigned count)
    {
        while (count--)
        {
            dest->~T();
            ++dest;
        }
    }
};

/// %Vector template class for POD types. Does not call constructors or destructors and uses block move.
template <class T> class PODVector : public VectorBase
{
public:
    typedef T ValueType;
    typedef RandomAccessIterator<T> Iterator;
    typedef RandomAccessConstIterator<T> ConstIterator;

    /// Construct empty.
    PODVector() : placement_(false) { }

    /// Construct with initial size.
    explicit PODVector(unsigned size) : placement_(false)
    {
        Resize(size);
    }

    /// Construct with initial size and default value.
    PODVector(unsigned size, const T& value) : placement_(false)
    {
        Resize(size);
        for (unsigned i = 0; i < size; ++i)
            At(i) = value;
    }

    /// Construct with initial data.
    PODVector(const T* data, unsigned size) : placement_(false)
    {
        Resize(size);
        CopyElements(Buffer(), data, size);
    }

    /// Construct from another vector.
    PODVector(const PODVector<T>& vector) : placement_(false)
    {
        *this = vector;
    }
#if URHO3D_CXX11
    /// Aggregate initialization constructor.
    PODVector(const std::initializer_list<T>& list) : PODVector()
    {
        for (auto it = list.begin(); it != list.end(); it++)
        {
            Push(*it);
        }
    }
#endif

    /// Placement :
    /// WARNING : To Use carefully, don't change datas just for casting and const
    /// U must be POD
    template< typename V > PODVector(const PODVector<V>& vector, bool placement) : placement_(placement)
    {
        if (placement)
        {
            size_ = vector.Size() * sizeof(V) / sizeof(ValueType);
            buffer_ = vector.GetRawBuffer();
        }
    }
    template <class U> PODVector<U> Placement() const
    {
        return PODVector<U>(*this, true);
    }

    /// Destruct.
    ~PODVector()
    {
        if (!placement_)
            delete[] buffer_;
    }

    /// Assign from another vector.
    PODVector<T>& operator =(const PODVector<T>& rhs)
    {
        // In case of self-assignment do nothing
        if (&rhs != this)
        {
            Resize(rhs.size_);
            if (rhs.size_)
                CopyElements(Buffer(), rhs.Buffer(), rhs.size_);
        }
        return *this;
    }

    /// Add-assign an element.
    PODVector<T>& operator +=(const T& rhs)
    {
        Push(rhs);
        return *this;
    }

    /// Add-assign another vector.
    PODVector<T>& operator +=(const PODVector<T>& rhs)
    {
        Push(rhs);
        return *this;
    }

    /// Add an element.
    PODVector<T> operator +(const T& rhs) const
    {
        PODVector<T> ret(*this);
        ret.Push(rhs);
        return ret;
    }

    /// Add another vector.
    PODVector<T> operator +(const PODVector<T>& rhs) const
    {
        PODVector<T> ret(*this);
        ret.Push(rhs);
        return ret;
    }

    /// Test for equality with another vector.
    bool operator ==(const PODVector<T>& rhs) const
    {
        if (rhs.size_ != size_)
            return false;

        T* buffer = Buffer();
        T* rhsBuffer = rhs.Buffer();
        for (unsigned i = 0; i < size_; ++i)
        {
            if (buffer[i] != rhsBuffer[i])
                return false;
        }

        return true;
    }

    /// Test for inequality with another vector.
    bool operator !=(const PODVector<T>& rhs) const
    {
        if (rhs.size_ != size_)
            return true;

        T* buffer = Buffer();
        T* rhsBuffer = rhs.Buffer();
        for (unsigned i = 0; i < size_; ++i)
        {
            if (buffer[i] != rhsBuffer[i])
                return true;
        }

        return false;
    }

    /// Return element at index.
    T& operator [](unsigned index)
    {
        assert(index < size_);
        return Buffer()[index];
    }

    /// Return const element at index.
    const T& operator [](unsigned index) const
    {
        assert(index < size_);
        return Buffer()[index];
    }

    /// Return element at index.
    T& At(unsigned index)
    {
        assert(index < size_);
        return Buffer()[index];
    }

    /// Return const element at index.
    const T& At(unsigned index) const
    {
        assert(index < size_);
        return Buffer()[index];
    }

    /// Add an element at the end.
    void Push(const T& value)
    {
        if (size_ < capacity_)
            ++size_;
        else
            Resize(size_ + 1);
        Back() = value;
    }

    /// Add another vector at the end.
    void Push(const PODVector<T>& vector)
    {
        unsigned oldSize = size_;
        Resize(size_ + vector.size_);
        CopyElements(Buffer() + oldSize, vector.Buffer(), vector.size_);
    }

    /// Add buffer at the end.
    void Push(const T* data, unsigned size)
    {
        unsigned oldSize = size_;
        Resize(size_ + size);
        CopyElements(Buffer() + oldSize, data, size);
    }

    /// Remove the last element.
    void Pop()
    {
        if (size_)
            Resize(size_ - 1);
    }

    /// Insert an element at position.
    void Insert(unsigned pos, const T& value)
    {
        if (pos > size_)
            pos = size_;

        unsigned oldSize = size_;
        Resize(size_ + 1);
        MoveRange(pos + 1, pos, oldSize - pos);
        Buffer()[pos] = value;
    }

    /// Insert another vector at position.
    void Insert(unsigned pos, const PODVector<T>& vector)
    {
        if (pos > size_)
            pos = size_;

        unsigned oldSize = size_;
        Resize(size_ + vector.size_);
        MoveRange(pos + vector.size_, pos, oldSize - pos);
        CopyElements(Buffer() + pos, vector.Buffer(), vector.size_);
    }

    /// Insert an element by iterator.
    Iterator Insert(const Iterator& dest, const T& value)
    {
        unsigned pos = (unsigned)(dest - Begin());
        if (pos > size_)
            pos = size_;
        Insert(pos, value);

        return Begin() + pos;
    }

    /// Insert a vector by iterator.
    Iterator Insert(const Iterator& dest, const PODVector<T>& vector)
    {
        unsigned pos = (unsigned)(dest - Begin());
        if (pos > size_)
            pos = size_;
        Insert(pos, vector);

        return Begin() + pos;
    }

    /// Insert a vector partially by iterators.
    Iterator Insert(const Iterator& dest, const ConstIterator& start, const ConstIterator& end)
    {
        unsigned pos = (unsigned)(dest - Begin());
        if (pos > size_)
            pos = size_;
        unsigned length = (unsigned)(end - start);
        Resize(size_ + length);
        MoveRange(pos + length, pos, size_ - pos - length);
        CopyElements(Buffer() + pos, &(*start), length);

        return Begin() + pos;
    }

    /// Insert elements.
    Iterator Insert(const Iterator& dest, const T* start, const T* end)
    {
        unsigned pos = (unsigned)(dest - Begin());
        if (pos > size_)
            pos = size_;
        unsigned length = (unsigned)(end - start);
        Resize(size_ + length);
        MoveRange(pos + length, pos, size_ - pos - length);

        T* destPtr = Buffer() + pos;
        for (const T* i = start; i != end; ++i)
            *destPtr++ = *i;

        return Begin() + pos;
    }

    /// Erase a range of elements.
    void Erase(unsigned pos, unsigned length = 1)
    {
        // Return if the range is illegal
        if (!length || pos + length > size_)
            return;

        MoveRange(pos, pos + length, size_ - pos - length);
        Resize(size_ - length);
    }

    /// Erase an element by iterator. Return iterator to the next element.
    Iterator Erase(const Iterator& it)
    {
        unsigned pos = (unsigned)(it - Begin());
        if (pos >= size_)
            return End();
        Erase(pos);

        return Begin() + pos;
    }

    /// Erase a range by iterators. Return iterator to the next element.
    Iterator Erase(const Iterator& start, const Iterator& end)
    {
        unsigned pos = (unsigned)(start - Begin());
        if (pos >= size_)
            return End();
        unsigned length = (unsigned)(end - start);
        Erase(pos, length);

        return Begin() + pos;
    }

    /// Erase a range of elements by swapping elements from the end of the array.
    void EraseSwap(unsigned pos, unsigned length = 1)
    {
        unsigned shiftStartIndex = pos + length;
        // Return if the range is illegal
        if (shiftStartIndex > size_ || !length)
            return;

        unsigned newSize = size_ - length;
        unsigned trailingCount = size_ - shiftStartIndex;
        if (trailingCount <= length)
        {
            // We're removing more elements from the array than exist past the end of the range being removed, so perform a normal shift and destroy
            MoveRange(pos, shiftStartIndex, trailingCount);
        }
        else
        {
            // Swap elements from the end of the array into the empty space
            CopyElements(Buffer() + pos, Buffer() + newSize, length);
        }
        Resize(newSize);
    }

    /// Erase an element by value. Return true if was found and erased.
    bool Remove(const T& value)
    {
        Iterator i = Find(value);
        if (i != End())
        {
            Erase(i);
            return true;
        }
        else
            return false;
    }

    /// Erase an element by value by swapping with the last element. Return true if was found and erased.
    bool RemoveSwap(const T& value)
    {
        Iterator i = Find(value);
        if (i != End())
        {
            EraseSwap(i - Begin());
            return true;
        }
        else
            return false;
    }

    /// Clear the vector.
    void Clear() { Resize(0); }

    /// Resize the vector.
    void Resize(unsigned newSize)
    {
        if (newSize > capacity_)
        {
            if (!capacity_)
                capacity_ = newSize;
            else
            {
                while (capacity_ < newSize)
                    capacity_ += (capacity_ + 1) >> 1;
            }

            unsigned char* newBuffer = AllocateBuffer((unsigned)(capacity_ * sizeof(T)));
            // Move the data into the new buffer and delete the old
            if (buffer_)
            {
                CopyElements(reinterpret_cast<T*>(newBuffer), Buffer(), size_);
                delete[] buffer_;
            }
            buffer_ = newBuffer;
        }

        size_ = newSize;
    }

    /// Set new capacity.
    void Reserve(unsigned newCapacity)
    {
        if (newCapacity < size_)
            newCapacity = size_;

        if (newCapacity != capacity_)
        {
            unsigned char* newBuffer = 0;
            capacity_ = newCapacity;

            if (capacity_)
            {
                newBuffer = AllocateBuffer((unsigned)(capacity_ * sizeof(T)));
                // Move the data into the new buffer
                CopyElements(reinterpret_cast<T*>(newBuffer), Buffer(), size_);
            }

            // Delete the old buffer
            delete[] buffer_;
            buffer_ = newBuffer;
        }
    }

    /// Reallocate so that no extra memory is used.
    void Compact() { Reserve(size_); }

    /// Return iterator to value, or to the end if not found.
    Iterator Find(const T& value)
    {
        Iterator it = Begin();
        while (it != End() && *it != value)
            ++it;
        return it;
    }

    /// Return const iterator to value, or to the end if not found.
    ConstIterator Find(const T& value) const
    {
        ConstIterator it = Begin();
        while (it != End() && *it != value)
            ++it;
        return it;
    }

    /// Return index of value in vector, or size if not found.
    unsigned IndexOf(const T& value) const
    {
        return Find(value) - Begin();
    }

    /// Return whether contains a specific value.
    bool Contains(const T& value) const { return Find(value) != End(); }

    /// Return iterator to the beginning.
    Iterator Begin() { return Iterator(Buffer()); }

    /// Return const iterator to the beginning.
    ConstIterator Begin() const { return ConstIterator(Buffer()); }

    /// Return iterator to the end.
    Iterator End() { return Iterator(Buffer() + size_); }

    /// Return const iterator to the end.
    ConstIterator End() const { return ConstIterator(Buffer() + size_); }

    /// Return first element.
    T& Front() { return Buffer()[0]; }

    /// Return const first element.
    const T& Front() const { return Buffer()[0]; }

    /// Return last element.
    T& Back()
    {
        assert(size_);
        return Buffer()[size_ - 1];
    }

    /// Return const last element.
    const T& Back() const
    {
        assert(size_);
        return Buffer()[size_ - 1];
    }

    /// Return number of elements.
    unsigned Size() const { return size_; }

    /// Return capacity of vector.
    unsigned Capacity() const { return capacity_; }

    /// Return whether vector is empty.
    bool Empty() const { return size_ == 0; }

    /// Return the buffer with right type.
    T* Buffer() const { return reinterpret_cast<T*>(buffer_); }

private:
    /// Move a range of elements within the vector.
    void MoveRange(unsigned dest, unsigned src, unsigned count)
    {
        if (count)
            memmove(Buffer() + dest, Buffer() + src, count * sizeof(T));
    }

    /// Copy elements from one buffer to another.
    static void CopyElements(T* dest, const T* src, unsigned count)
    {
        if (count)
            memcpy(dest, src, count * sizeof(T));
    }

    bool placement_;
};

template <class T> typename Urho3D::Vector<T>::ConstIterator begin(const Urho3D::Vector<T>& v) { return v.Begin(); }

template <class T> typename Urho3D::Vector<T>::ConstIterator end(const Urho3D::Vector<T>& v) { return v.End(); }

template <class T> typename Urho3D::Vector<T>::Iterator begin(Urho3D::Vector<T>& v) { return v.Begin(); }

template <class T> typename Urho3D::Vector<T>::Iterator end(Urho3D::Vector<T>& v) { return v.End(); }

template <class T> typename Urho3D::PODVector<T>::ConstIterator begin(const Urho3D::PODVector<T>& v) { return v.Begin(); }

template <class T> typename Urho3D::PODVector<T>::ConstIterator end(const Urho3D::PODVector<T>& v) { return v.End(); }

template <class T> typename Urho3D::PODVector<T>::Iterator begin(Urho3D::PODVector<T>& v) { return v.Begin(); }

template <class T> typename Urho3D::PODVector<T>::Iterator end(Urho3D::PODVector<T>& v) { return v.End(); }

}

#include <typeinfo>
#include <cstring>

namespace Urho3D
{

/// C.VILLE for FromBones Project

/// Heterogen Storage Class (store different object types).
/// Warning : this class has no continuous allocation, just like a list
///           don't use it for huge collection of object
/// TODO    : reserved allocation memory

/// Used in VkGraphicsImpl

//const unsigned COLLECTION_DEFAULTBYTESIZE = 10000U;

class Collection
{
private:
    struct Base
    {
        virtual ~Base() { }
    };

    template<typename T> struct Derived : Base
    {
        Derived(T* ptr) : ptr_(ptr) { }
        T* ptr_;
    };

public:
    Collection()
    {
        // Reserve default size for the memory
//        capacity_ = COLLECTION_DEFAULTBYTESIZE;
//        buffer_ = ::new[COLLECTION_DEFAULTBYTESIZE] unsigned char;
    }

    Collection(unsigned bytesize)
    {
        // Reserve default size for the memory
//        capacity_ = bytesize;
//        buffer_ = ::new[bytesize] unsigned char;
    }

    ~Collection()
    {
        Clear();
    }

    void Clear()
    {
        for (Vector<Base* >::Iterator it = storage_.Begin(); it != storage_.End(); ++it)
            delete *it;

        storage_.Clear();
    }

    template<typename T> T& New()
    {
        T* ptr = new T;
        storage_.Push(new Derived<T>(ptr));
        return *ptr;
    }

    template<typename T> T& Push(const T& value)
    {
        T& object = New<T>();
        // copy
        object = value;
        return object;
    }

    // find the first object of Type T in the collection
    template<typename T> T* Find() const
    {
        for (Vector<Base* >::ConstIterator it = storage_.Begin(); it != storage_.End(); ++it)
        {
            if (typeid(**it) == typeid(Derived<T>))
            {
                return static_cast<Derived<T>* >(*it)->ptr_;
            }
        }
        return 0;
    }

    template<typename T> void FindAll(PODVector<T*>& results) const
    {
        for (Vector<Base* >::ConstIterator it = storage_.Begin(); it != storage_.End(); ++it)
        {
            if (typeid(**it) == typeid(Derived<T>))
                results.Push(static_cast<Derived<T>* >(*it)->ptr_);
        }
    }

    template<typename T> bool IsTypeAtIndex(unsigned index) const
    {
        if (index >= Size())
            return false;

        return typeid(*storage_[index]) == typeid(Derived<T>);
    }

    const char* GetTypeAt(unsigned index) const
    {
        static char buffer[512];
        const std::type_info& typeinfo = typeid(*storage_[index]);
        const unsigned length = (unsigned)strlen(typeinfo.name()) - 2 - CollectionTypeNameStartIndex;
        memcpy(buffer, &typeinfo.name()[CollectionTypeNameStartIndex], length);
        return buffer;
//        return typeid(*(static_cast<Derived<Base>* >(storage_[index])->ptr_)).name();
    }

    void* At(unsigned i)
    {
        return static_cast<Derived<void>* >(storage_[i])->ptr_;
    }

    void* Front()
    {
        return At(0);
    }

    void* Back()
    {
        return At(Size() - 1);
    }

    template<typename T> T& At(unsigned i)
    {
        return static_cast<Derived<T>* >(storage_[i])->ptr_;
    }

    template<typename T> T& Front()
    {
        return At<T>(0);
    }

    template<typename T> T& Back()
    {
        return At<T>(Size() - 1);
    }

    unsigned Size() const
    {
        return storage_.Size();
    }

private:
    Vector<Base* > storage_;
//    unsigned size_;
//    unsigned capacity_;
//    unsigned char* buffer_;
    static const unsigned CollectionTypeNameStartIndex = 31U; //"N6Urho3D10Collection7DerivedI42"
};

}
#ifdef _MSC_VER
#pragma warning(pop)
#endif
