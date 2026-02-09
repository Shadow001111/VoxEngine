#pragma once
#include "ImmutableBuffer.h"

template<size_t FramesCount = 3>
class PersistentRingBuffer
{
    ImmutableBuffer buffer;
    size_t currentFrame = 0;
    GLsync frameFences[FramesCount];
public:
    PersistentRingBuffer() = default;
    
    ~PersistentRingBuffer()
    {
        destroy();
    }

    void create(GLenum target)
    {
        buffer.create(target);
        for (auto& fence : frameFences)
        {
            fence = nullptr;
        }
    }

    void allocateStorage(size_t size, GLbitfield flags, const void* data = nullptr)
    {
        buffer.allocateStorage(size * FramesCount, flags | GL_MAP_PERSISTENT_BIT | GL_MAP_WRITE_BIT | GL_MAP_COHERENT_BIT, data);
    }

    void destroy()
    {
        for (auto& fence : frameFences)
        {
            if (fence)
            {
                glDeleteSync(fence);
                fence = nullptr;
            }
        }
        buffer.destroy();
    }

    void map()
    {
        buffer.mapPersistent(GL_MAP_WRITE_BIT | GL_MAP_COHERENT_BIT); // GL_MAP_PERSISTENT_BIT is included in 'mapPersistent'
    }

    // Wait for a specific frame to be free (no longer in use by GPU)
    bool waitForFrame(size_t frameIndex, GLuint64 timeout = 1000000000) // 1 second default timeout
    {
        if (frameIndex >= FramesCount) return false;

        if (!frameFences[frameIndex])
        {
            return true; // No fence means frame is free
        }

        GLenum waitResult = glClientWaitSync(
            frameFences[frameIndex],
            GL_SYNC_FLUSH_COMMANDS_BIT,
            timeout
        );

        // Clean up the fence if we successfully waited
        if (waitResult == GL_ALREADY_SIGNALED || waitResult == GL_CONDITION_SATISFIED)
        {
            glDeleteSync(frameFences[frameIndex]);
            frameFences[frameIndex] = nullptr;
            return true;
        }

        return (waitResult == GL_ALREADY_SIGNALED || waitResult == GL_CONDITION_SATISFIED);
    }

    // Wait for the current frame (the one we're about to write to)
    bool waitForCurrentFrame(GLuint64 timeout = 1000000000)
    {
        return waitForFrame(currentFrame, timeout);
    }

    void* getWritePointer()
    {
        // Wait for the current frame to be free before returning write pointer
        waitForCurrentFrame();

        if (void* ptr = buffer.getPersistentMappedPtr())
        {
            return static_cast<char*>(ptr) + currentFrame * getFrameSize();
        }
        return nullptr;
    }

    void writeCurrentFrame(const void* data, size_t size)
    {
        // Wait for the current frame to be free
        waitForCurrentFrame();

        size_t offset = currentFrame * getFrameSize();

        if (void* mappedPtr = buffer.getPersistentMappedPtr())
        {
            std::memcpy(static_cast<char*>(mappedPtr) + offset, data, size);
            buffer.flushMappedRange(offset, size);
        }
        else
        {
            buffer.write(data, size, offset);
        }
    }

    // Call this after submitting commands that use the current frame
    void insertFrameFence()
    {
        // Clean up any existing fence for this frame
        if (frameFences[currentFrame])
        {
            glDeleteSync(frameFences[currentFrame]);
        }

        // Insert a fence after all commands that use this frame
        frameFences[currentFrame] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        glFlush(); // Ensure commands are submitted
    }

    void advanceFrame()
    {
        // Insert fence for the frame we just finished using
        insertFrameFence();

        currentFrame = (currentFrame + 1) % FramesCount;
    }

    GLuint getID() const { return buffer.getID(); }
    size_t getFrameSize() const { return buffer.getCapacity() / FramesCount; }
    size_t getCurrentFrameIndex() const { return currentFrame; }
    const ImmutableBuffer& getBuffer() const { return buffer; }
    ImmutableBuffer& getBuffer() { return buffer; }
};