/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/ZLMediaKit/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLTOOLKIT_LIST_H
#define ZLTOOLKIT_LIST_H

#include <list>
#include <type_traits>

namespace toolkit {

#if 0
template<typename T>
class List;

template<typename T>
class ListNode
{
public:
    friend class List<T>;
    ~ListNode(){}

    template <class... Args>
    ListNode(Args&&... args):_data(std::forward<Args>(args)...){}

private:
    T _data;
    ListNode *next = nullptr;
};


template<typename T>
class List {
public:
    using NodeType = ListNode<T>;
    List(){}
    List(List &&that){
        swap(that);
    }
    ~List(){
        clear();
    }
    void clear(){
        auto ptr = _front;
        auto last = ptr;
        while(ptr){
            last = ptr;
            ptr = ptr->next;
            delete last;
        }
        _size = 0;
        _front = nullptr;
        _back = nullptr;
    }

    template<typename FUN>
    void for_each(FUN &&fun) const {
        auto ptr = _front;
        while (ptr) {
            fun(ptr->_data);
            ptr = ptr->next;
        }
    }

    size_t size() const{
        return _size;
    }

    bool empty() const{
        return _size == 0;
    }
    template <class... Args>
    void emplace_front(Args&&... args){
        NodeType *node = new NodeType(std::forward<Args>(args)...);
        if(!_front){
            _front = node;
            _back = node;
            _size = 1;
        }else{
            node->next = _front;
            _front = node;
            ++_size;
        }
    }

    template <class...Args>
    void emplace_back(Args&&... args){
        NodeType *node = new NodeType(std::forward<Args>(args)...);
        if(!_back){
            _back = node;
            _front = node;
            _size = 1;
        }else{
            _back->next = node;
            _back = node;
            ++_size;
        }
    }

    T &front() const{
        return _front->_data;
    }

    T &back() const{
        return _back->_data;
    }

    T &operator[](size_t pos){
        NodeType *front = _front ;
        while(pos--){
            front = front->next;
        }
        return front->_data;
    }

    void pop_front(){
        if(!_front){
            return;
        }
        auto ptr = _front;
        _front = _front->next;
        delete ptr;
        if(!_front){
            _back = nullptr;
        }
        --_size;
    }

    void swap(List &other){
        NodeType *tmp_node;

        tmp_node = _front;
        _front = other._front;
        other._front = tmp_node;

        tmp_node = _back;
        _back = other._back;
        other._back = tmp_node;

        size_t tmp_size = _size;
        _size = other._size;
        other._size = tmp_size;
    }

    void append(List<T> &other){
        if(other.empty()){
            return;
        }
        if(_back){
            _back->next = other._front;
            _back = other._back;
        }else{
            _front = other._front;
            _back = other._back;
        }
        _size += other._size;

        other._front = other._back = nullptr;
        other._size = 0;
    }

    List &operator=(const List &that) {
        that.for_each([&](const T &t) {
            emplace_back(t);
        });
        return *this;
    }

private:
    NodeType *_front = nullptr;
    NodeType *_back = nullptr;
    size_t _size = 0;
};

#else

template<typename T>
class List : public std::list<T> {
public:
    template<typename ... ARGS>
    List(ARGS &&...args) : std::list<T>(std::forward<ARGS>(args)...) {};

    ~List() = default;

    void append(List<T> &other) {
        if (other.empty()) {
            return;
        }
        this->insert(this->end(), other.begin(), other.end());
        other.clear();
    }

    template<typename FUNC>
    void for_each(FUNC &&func) {
        for (auto &t : *this) {
            func(t);
        }
    }

    template<typename FUNC>
    void for_each(FUNC &&func) const {
        for (auto &t : *this) {
            func(t);
        }
    }
};

#endif

} /* namespace toolkit */
#endif //ZLTOOLKIT_LIST_H
