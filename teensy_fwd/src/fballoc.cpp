#ifndef YAHP_FBALLOC
#define YAHP_FBALLOC

// ripped off from https://github.com/google/flatbuffers/issues/5135#issuecomment-542088960
// because I'm lazy

#include "flatbuffers/allocator.h"
#include "utils.cpp"
#include <cstddef>
#include <cstdint>

/// [bytes] Minimum size that an allocator needs to properly operate.
//const size_t FLATBUFFERS_MIN_BUFFER_SIZE = 1024U;

/**
 * @brief Custom flatbuffer allocator without dynamic memory allocation.
 * Only allows a single pointer to be handed out.
 */
class FlatbufferStaticAllocator_c : public flatbuffers::Allocator {
public:
  uint8_t *const
      m_buffer; ///< Pointer to the buffer that is available for allocation.
  const size_t
      m_bufferSize; ///< [bytes] Size of the buffer available for allocation.
  size_t m_currentSize = 0; ///< [bytes] Currently allocated size.
  bool m_bufferInUse =
      false; ///< Keeps track if the buffer has already been allocated.

  /**
   * @brief Construct a new FlatbufferStaticAllocator c object
   * @param bufferIn Pointer to the buffer that is used for allocation
   * @param bufferSizeIn Size of the buffer, determines the size of maximum
   * allocation
   */
  FlatbufferStaticAllocator_c(uint8_t *const bufferIn,
                              const size_t bufferSizeIn)
      : m_buffer(bufferIn), m_bufferSize(bufferSizeIn) {
    if (bufferSizeIn < FLATBUFFERS_MIN_BUFFER_SIZE) {
      eloop("Flatbuffer allocator: Buffer too small.");
    }
  }

  /**
   * @brief Allocate a new piece of memory for use
   * @param size [bytes] Size of the requested memory blob
   * @return uint8_t* Pointer to reserved memory with size of #size
   */
  uint8_t *allocate(size_t size) {
    if (size <= m_bufferSize && !m_bufferInUse) {
      m_bufferInUse = true;
      m_currentSize = size;
      return m_buffer;
    } else {
      eloop("Flatbuffer allocator: allocation request cannot be handled, out "
            "of "
            "memory.");
      return nullptr;
    }
  }

  /**
   * @brief Deallocate a piece of memory so it can be used again
   * @param p Pointer to memory to deallocate
   * @param size [bytes] Size of memory to deallocate
   */
  void deallocate(uint8_t *p, size_t size) {
    /**
     * Check if the deallocation request matches with the allocated data
     */
    FLATBUFFERS_ASSERT(p == m_buffer);
    FLATBUFFERS_ASSERT(size == m_currentSize);
    FLATBUFFERS_ASSERT(m_bufferInUse);
    if (p != m_buffer || size != m_currentSize || !m_bufferInUse) {
      eloop("Flatbuffer allocator: deallocation requested with wrong pointer "
            "or "
            "size.");
    }

    /// - Set buffer to all zeroes on deallocate to ensure a fresh start
    memset(m_buffer, 0, m_bufferSize);
    m_bufferInUse = false;
    m_currentSize = 0;
  }

  /**
   * @brief Move old data to a new piece of memory that is larger than the
   * previous
   * @param old_p Pointer to memory where current contens are
   * @param old_size [bytes] Size of memory blob that has current contents
   * @param new_size [bytes] New size of memory blob
   * @param in_use_back [bytes] Size of data in use at the back of the current
   * memory blob
   * @param in_use_front [bytes] Size of data in use at the front of the
   * current memory blob
   * @return uint8_t* Pointer to new memory blob with size #new_size and with
   * same contents, i.e. data is added in the middle
   */
  uint8_t *reallocate_downward(uint8_t *old_p, size_t old_size, size_t new_size,
                               size_t in_use_back, size_t in_use_front) {
    /**
     * Allocate a new piece of memory and copy the data at the end of the old
     * region to the new region. The front is not necessary as the allocator
     * only hands out one pointer.
     */
    if (new_size <= m_bufferSize && new_size > old_size) {
      m_bufferInUse = false;
      m_currentSize = 0;
      uint8_t *new_p = allocate(new_size);
      memcpy(new_p + new_size - in_use_back, old_p + old_size - in_use_back,
             in_use_back);
      return new_p;
    } else {
      eloop("Flatbuffer allocator: allocation request cannot be handled, out "
            "of "
            "memory.");
      return nullptr;
    }
  }
};

#endif
