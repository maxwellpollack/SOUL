/*
    _____ _____ _____ __
   |   __|     |  |  |  |      The SOUL language
   |__   |  |  |  |  |  |__    Copyright (c) 2019 - ROLI Ltd.
   |_____|_____|_____|_____|

   The code in this file is provided under the terms of the ISC license:

   Permission to use, copy, modify, and/or distribute this software for any purpose
   with or without fee is hereby granted, provided that the above copyright notice and
   this permission notice appear in all copies.

   THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD
   TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN
   NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
   DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER
   IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
   CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

namespace soul
{

//==============================================================================
/**
    Abstract base class for a "performer" which can compile and execute a soul::Program.

    A typical performer is likely to be a JIT compiler or an interpreter.

    Note that performer implementations are not expected to be thread-safe!
    Performers will typically not create any internal threads, and all its methods
    are synchronous (for an asynchronous playback engine, see soul::Venue).
    Any code which uses a performer is responsible for making sure it calls the methods
    in a race-free way, and takes into account the fact that some of the calls may block
    for up to a few seconds.
*/
class Performer
{
public:
    virtual ~Performer() {}

    /** Provides the program for the performer to load.
        If a program is already loaded or linked, calling this should reset the state
        before attempting to load the new one.
        After successfully loading a program, the caller should then connect getter/setter
        callback to any endpoints that it wants to communicate with, and then call link()
        to prepare it for use.
        Note that this method blocks until building is finished, and it's not impossible
        that an optimising JIT engine could take up to several seconds, so make sure
        the caller takes this into account.
        Returns true on success; on failure, the CompileMessageList should contain error
        messages describing what went wrong.
    */
    virtual bool load (CompileMessageList&, const Program& programToLoad) = 0;

    /** Unloads any currently loaded program, and resets the state of the performer. */
    virtual void unload() = 0;

    /** When a program has been loaded, this returns a list of the input endpoints that
        the program provides.
    */
    virtual ArrayView<const EndpointDetails> getInputEndpoints() = 0;

    /** When a program has been loaded, this returns a list of the output endpoints that
        the program provides.
    */
    virtual ArrayView<const EndpointDetails> getOutputEndpoints() = 0;

    struct ExternalVariable
    {
        std::string name;
        Type type;
        Annotation annotation;
    };

    /** Returns the list of external variables that need to be resolved before a loaded
        program can be linked.
    */
    virtual ArrayView<const ExternalVariable> getExternalVariables() = 0;

    /** Add a global constant to the loaded program. */
    virtual ConstantTable::Handle addConstant (Value value) = 0;

    /** Set the value of an external in the loaded program. */
    virtual bool setExternalVariable (const char* name, Value value) = 0;

    /** After loading a program, and optionally connecting up to some of its endpoints,
        link() will complete any preparations needed before the code can be executed.
        If this returns true, then you can safely start calling advance(). If it
        returns false, the error messages will be added to the CompileMessageList
        provided.
        Note that this method blocks until building is finished, and it's not impossible
        that an optimising JIT engine could take up to several seconds, so make sure
        the caller takes this into account.
    */
    virtual bool link (CompileMessageList&, const LinkOptions&, LinkerCache*) = 0;

    /** Returns true if a program is currently loaded. */
    virtual bool isLoaded() = 0;

    /** Returns true if a program is successfully linked and ready to execute. */
    virtual bool isLinked() = 0;

    /** Resets the performer to the state it was in when freshly linked.
        This doesn't unlink or unload the program, it simply resets the program's
        internal state so that the next advance() call will begin a fresh run.
    */
    virtual void reset() = 0;

    /** When a program has been loaded (but not yet linked), this returns
        a handle that can be used later by other methods which need to reference
        an input or output endpoint.
        Will return a null handle if the ID is not found.
    */
    virtual EndpointHandle getEndpointHandle (const EndpointID&) = 0;

    /** Indicates that a block of frames is going to be rendered.

        Once a program has been loaded and linked, a caller will typically make repeated
        calls to prepare() and advance() to actually perform the rendering work.
        Between calls to prepare() and advance(), the caller must fill input buffers with the
        content needed to render the number of frames requested here. Then advance() can be
        called, after which the prepared number of frames of output are ready to be read.
        The value of numFramesToBeRendered must not exceed the block size specified when linking.
        Because you're likely to be calling advance() from an audio thread, be careful not to
        allow any calls to other methods such as unload() to overlap with calls to advance()!
    */
    virtual void prepare (uint32_t numFramesToBeRendered) = 0;

    /** Callback function used by iterateOutputEvents().
        The frameOffset is relative to the start of the last block that was rendered during advance().
        @returns true to continue iterating, or false to stop.
    */
    using HandleNextOutputEventFn = std::function<bool(uint32_t frameOffset, const Value& event)>&&;

    /** Pushes a block of samples to an input endpoint.
        After a successful call to prepare(), and before a call to advance(), this should be called
        to provide the next block of samples for an input stream. The value provided should be an
        array of as many frames as was specified in prepare(). If this is called more than once before
        advance(), only the most recent value is used.
        The EndpointHandle is obtained by calling getEndpointHandle().
    */
    virtual void setNextInputStreamFrames (EndpointHandle, const Value& frameArray) = 0;

    /** Sets the next levels for a sparse-stream input.
        After a successful call to prepare(), and before a call to advance(), this should be called
        to set the trajectory for a sparse input stream over the next block. If this is called more
        than once before advance(), only the most recent value is used.
        The EndpointHandle is obtained by calling getEndpointHandle().
    */
    virtual void setSparseInputStreamTarget (EndpointHandle, const Value& targetFrameValue,
                                             uint32_t numFramesToReachValue, float curveShape) = 0;

    /** Sets a new value for a value input.
        After a successful call to prepare(), and before a call to advance(), this may be called
        to set a new value for a value input. If this is called more than once before advance(),
        only the most recent value is used.
        The EndpointHandle is obtained by calling getEndpointHandle().
    */
    virtual void setInputValue (EndpointHandle, const Value& newValue) = 0;

    /** Adds an event to an input queue.
        After a successful call to prepare(), and before a call to advance(), this may be called
        multiple times to add events for an event input endpoint. During the next call to advance,
        all the events that were added will be dispatched in order, and the queue will be reset.
        The EndpointHandle is obtained by calling getEndpointHandle().
    */
    virtual void addInputEvent (EndpointHandle, const Value& eventData) = 0;

    /** Retrieves the most recent block of frames from an output stream.
        After a successful call to advance(), this may be called to get the block of frames which
        were rendered during that call. A nullptr return value indicates an error.
    */
    virtual const Value* getOutputStreamFrames (EndpointHandle) = 0;

    /** Retrieves the last block of events which were emitted by an event output.
        After a successful call to advance(), this may be called to iterate the list of events
        which the program emitted on the given endpoint. The callback function provides the
        frame offset and content of each event.
    */
    virtual void iterateOutputEvents (EndpointHandle, HandleNextOutputEventFn) = 0;

    /** Renders the next block of frames.

        Once the caller has called prepare(), a call to advance() will synchronously render the next
        block of frames. If any inputs have not been correctly populated, over- and under-runs
        may occur and the associated counters will be incremented to reflect this.
    */
    virtual void advance() = 0;

    /** Returns true if something has got a handle to this endpoint and might be using it
        during the current program run.
    */
    virtual bool isEndpointActive (const EndpointID&) = 0;

    /** Returns the number of over- or under-runs that have happened since the program was linked.
        Underruns can happen when an endpoint callback fails to empty or fill the amount of data
        that it is asked to handle.
    */
    virtual uint32_t getXRuns() = 0;

    /** Returns the block size which is the maximum number of frames that can be rendered in one
        prepare call.
    */
    virtual uint32_t getBlockSize() = 0;
};


//==============================================================================
/**
    Abstract base class for a factory which can construct Performers
*/
class PerformerFactory
{
public:
    virtual ~PerformerFactory() {}

    virtual std::unique_ptr<Performer> createPerformer() = 0;
};

} // namespace soul
