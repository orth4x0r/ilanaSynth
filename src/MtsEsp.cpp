#include "MtsEsp.h"

#include "thirdparty/mts-esp/libMTSClient.cpp"

MtsEspClient::MtsEspClient() : client (MTS_RegisterClient()) {}

MtsEspClient::~MtsEspClient()
{
    if (client != nullptr)
        MTS_DeregisterClient (static_cast<MTSClient*> (client));
}

bool MtsEspClient::hasMaster() const
{
    return client != nullptr && MTS_HasMaster (static_cast<MTSClient*> (client));
}

double MtsEspClient::noteToFrequency (int midiNote, int midiChannel) const
{
    return MTS_NoteToFrequency (static_cast<MTSClient*> (client), (char) midiNote, (signed char) midiChannel);
}

bool MtsEspClient::shouldFilterNote (int midiNote, int midiChannel) const
{
    return MTS_ShouldFilterNote (static_cast<MTSClient*> (client), (char) midiNote, (signed char) midiChannel);
}

const char* MtsEspClient::scaleName() const
{
    return client != nullptr ? MTS_GetScaleName (static_cast<MTSClient*> (client)) : "";
}
