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
template <typename ArrayType>
const EndpointDetails& findDetailsForID (const ArrayType& endpoints, const EndpointID& endpointID)
{
    for (auto& e : endpoints)
        if (e.endpointID == endpointID)
            return e;

    SOUL_ASSERT_FALSE;
    return endpoints.front();
}

template <typename ArrayType>
bool containsEndpoint (const ArrayType& endpoints, const EndpointID& endpointID)
{
    for (auto& e : endpoints)
        if (e.endpointID == endpointID)
            return true;

    return false;
}

template <typename VenueOrPerformer>
EndpointID findFirstInputOfType (VenueOrPerformer& p, EndpointKind kind)
{
    for (auto& i : p.getInputEndpoints())
        if (i.kind == kind)
            return i.endpointID;

    return {};
}

template <typename VenueOrPerformer>
EndpointID findFirstOutputOfType (VenueOrPerformer& p, EndpointKind kind)
{
    for (auto& o : p.getOutputEndpoints())
        if (o.kind == kind)
            return o.endpointID;

    return {};
}

bool isMIDIMessageStruct (const Type&);
bool isMIDIEventEndpoint (const EndpointDetails&);
Type createMIDIEventEndpointType();
bool isParameterInput (const EndpointDetails&);
bool isConsoleEndpoint (const std::string& endpointName);

} // namespace soul
