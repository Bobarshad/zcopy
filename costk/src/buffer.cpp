#include "extern.h"

Buffer::Buffer(BufferPool *p) {
   pool = p;
}

Buffer::~Buffer() {
}

BufferPool *
Buffer::getPool() {
   return pool;
}

void
Buffer::returnToPool() {
   pool->poolBuffer(this);
}
