// Test2.cpp : �������̨Ӧ�ó������ڵ㡣
//

#include <stdio.h>
#include <chrono>
#include <memory>

struct A{
    typedef std::shared_ptr<A> Ptr;
    int a = 0;
    int b = 0;
    int c = 0;
    //char buf[256];
};

A Func0(A a){ a.a++; return a; }
A* Func1(A* a){ a->a++; return a; }
A::Ptr Func2(A::Ptr a){ a->b++; return a; }
A::Ptr Func3(A::Ptr a){ a->c++; return std::move(a); }
/*
in i5 7400 / 8g memory / vs2013 build 
with buf:
Func0 1000000 use 32495 ms
Func1 2000000 use 1500 ms
Func2.1 1000000 use 19499 ms
Func2.2 2000000 use 6999 ms
Func3.1 1000000 use 19000 ms
Func3.2 2000000 use 6501 ms

without buf
Func0 1000000 use 10001 ms
Func1 2000000 use 2014 ms
Func2.1 1000000 use 19483 ms
Func2.2 2000000 use 7000 ms
Func3.1 1000000 use 19500 ms
Func3.2 2000000 use 6999 ms
*/
int main(int argc, char* argv[])
{
    int count = 1000000;
    if (argc > 1) count = atoi(argv[1]);
    int i = 0;
    auto sptr = std::make_shared<A>();
    A* ptr = sptr.get();
    using clock = std::chrono::high_resolution_clock;
    clock::time_point end,tp = clock::now();
    for (i = 0; i < count; i++)
    {
        *ptr = Func0(*ptr);
    }
    end = clock::now();
    printf("Func0 %d use %lld ms\n", ptr->a, std::chrono::duration_cast<std::chrono::microseconds>(end - tp).count());
    tp = end;

    for (i = 0; i < count; i++)
    {
        ptr = Func1(ptr);
    }
    end = clock::now();
    printf("Func1 %d use %lld ms\n", ptr->a, std::chrono::duration_cast<std::chrono::microseconds>(end - tp).count());
    tp = end;

    for (i = 0; i < count; i++)
    {
        sptr = Func2(sptr);
    }
    end = clock::now();
    printf("Func2.1 %d use %lld ms\n", ptr->b, std::chrono::duration_cast<std::chrono::microseconds>(end - tp).count());
    tp = end;

    for (i = 0; i < count; i++)
    {
        sptr = Func2(std::move(sptr));
    }
    end = clock::now();
    printf("Func2.2 %d use %lld ms\n", ptr->b, std::chrono::duration_cast<std::chrono::microseconds>(end - tp).count());
    tp = end;

    for (i = 0; i < count; i++)
    {
        sptr = Func3(sptr);
    }
    end = clock::now();
    printf("Func3.1 %d use %lld ms\n", ptr->c, std::chrono::duration_cast<std::chrono::microseconds>(end - tp).count());
    tp = end;

    for (i = 0; i < count; i++)
    {
        sptr = Func3(std::move(sptr));
    }
    end = clock::now();
    printf("Func3.2 %d use %lld ms\n", ptr->c, std::chrono::duration_cast<std::chrono::microseconds>(end - tp).count());
    tp = end;
    // getchar();
    return 0;
}

