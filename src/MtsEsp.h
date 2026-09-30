#pragma once

// MTS-ESP (ODDSound): a microtuning master plugin in the session (MTS-ESP
// Mini, Scala tuners, Wilsonic, ...) retunes every client live. This wraps
// the client library (src/thirdparty/mts-esp, free licence) so its
// windows.h stays in MtsEsp.cpp. With no master (the library isn't
// installed, or no master is loaded) every call is cheap and says so.

class MtsEspClient
{
public:
    MtsEspClient();
    ~MtsEspClient();

    MtsEspClient (const MtsEspClient&) = delete;
    MtsEspClient& operator= (const MtsEspClient&) = delete;

    bool hasMaster() const;
    double noteToFrequency (int midiNote, int midiChannel = -1) const;
    bool shouldFilterNote (int midiNote, int midiChannel = -1) const;
    const char* scaleName() const;

private:
    void* client = nullptr;
};
