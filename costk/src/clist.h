#ifndef __LOG_CLIST__
#define __LOG_CLIST__

#include <list>
#include "condition.h"

template <class T>
class CList : Semaphore {
private:
   list<T>   mData;
   int       mLength;
   Condition mNotEmpty;

public:
   CList()               {mLength=0;}
   CList(const T& item)  {mLength=0; push_back(item);}
   virtual ~CList()      {}

   inline int  length()  {return mLength;}
   inline bool isEmpty() {return (mLength == 0);}

   inline T    front()                   {T tmp; P(); tmp = mData.front(); V(); return tmp;}
   inline void pop_front()               {P(); if (mLength > 0) {mData.pop_front(); mLength--;} V();}

   inline void push_front(const T& data) {P(); mData.push_front(data); mLength++; mNotEmpty.raise(); V();}
   inline void push_back(const T& data)  {P(); mData.push_back(data);  mLength++; mNotEmpty.raise(); V();}

   inline void waitFor()                 {mNotEmpty.waitFor();}
   inline void clear()                   {P(); mData.clear(); mLength=0; mNotEmpty.reset(); V();}

   inline void remove(const T& data)     {P(); mData.remove(data); mLength--; V();}
};

#endif
