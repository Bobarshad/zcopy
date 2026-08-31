#ifndef __BUFFER_INC__
#define __BUFFER_INC__

#include "extern.h"

#define PACKET_LEN  16384

class BufferPool;

class Buffer {
public:
   uint16_t length;
   uint16_t offset;
   char     data[PACKET_LEN];
   BufferPool *pool;

   Buffer(BufferPool *pool);
   ~Buffer();

   BufferPool *getPool();
   void returnToPool();
};

#endif
