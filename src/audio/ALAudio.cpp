#include "ALAudio.h"
#include "alutil.h"
#include <string>
#include <iostream>

using namespace audio;

ALSound::ALSound(ALAudio* al, uint buffer, std::shared_ptr<PCM> pcm, bool keepPCM) 
: al(al), buffer(buffer) 
{
    duration = pcm->getDuration();
    if (keepPCM) {
        this->pcm = pcm;
    }
}

ALSound::~ALSound() {
    al->freeBuffer(buffer);
    buffer = 0;
}

Speaker* ALSound::newInstance(int priority) const {
    uint source = al->getFreeSource();
    if (source == 0) {
        return nullptr;
    }
    AL_CHECK(alSourcei(source, AL_BUFFER, buffer));
    return new ALSpeaker(al, source, priority);
}

ALSpeaker::ALSpeaker(ALAudio* al, uint source, int priority) : al(al), source(source), priority(priority) {
}

ALSpeaker::~ALSpeaker() {
    if (source) {
        stop();
    }
}

State ALSpeaker::getState() const {
    int state = AL::getSourcei(source, AL_SOURCE_STATE, AL_STOPPED);
    switch (state) {
        case AL_PLAYING: return State::playing;
        case AL_PAUSED: return State::paused;
        default: return State::stopped;
    }
}

float ALSpeaker::getVolume() const {
    return AL::getSourcef(source, AL_GAIN);
}

void ALSpeaker::setVolume(float volume) {
    AL_CHECK(alSourcef(source, AL_GAIN, volume));
}

float ALSpeaker::getPitch() const {
    return AL::getSourcef(source, AL_PITCH);
}

void ALSpeaker::setPitch(float pitch) {
    AL_CHECK(alSourcef(source, AL_PITCH, pitch));
}

bool ALSpeaker::isLoop() const {
    return AL::getSourcei(source, AL_LOOPING) == AL_TRUE;
}

void ALSpeaker::setLoop(bool loop) {
    AL_CHECK(alSourcei(source, AL_LOOPING, loop ? AL_TRUE : AL_FALSE));
}

void ALSpeaker::play() {
    AL_CHECK(alSourcePlay(source));
}

void ALSpeaker::pause() {
    AL_CHECK(alSourcePause(source));
}

void ALSpeaker::stop() {
    AL_CHECK(alSourceStop(source));
    al->freeSource(source);
    source = 0;
}

duration_t ALSpeaker::getTime() const {
    return static_cast<duration_t>(AL::getSourcef(source, AL_SEC_OFFSET));
}

void ALSpeaker::setTime(duration_t time) {
    AL_CHECK(alSourcef(source, AL_SEC_OFFSET, static_cast<float>(time)));
}

void ALSpeaker::setPosition(glm::vec3 pos) {
    AL_CHECK(alSource3f(source, AL_POSITION, pos.x, pos.y, pos.z));
}

glm::vec3 ALSpeaker::getPosition() const {
    return AL::getSource3f(source, AL_POSITION);
}

void ALSpeaker::setVelocity(glm::vec3 vel) {
    AL_CHECK(alSource3f(source, AL_VELOCITY, vel.x, vel.y, vel.z));
}

glm::vec3 ALSpeaker::getVelocity() const {
    return AL::getSource3f(source, AL_VELOCITY);
}

int ALSpeaker::getPriority() const {
    return priority;
}


ALAudio::ALAudio(ALCdevice* device, ALCcontext* context)
: device(device), context(context)
{
    ALCint size;
    alcGetIntegerv(device, ALC_ATTRIBUTES_SIZE, 1, &size);
    std::vector<ALCint> attrs(size);
    alcGetIntegerv(device, ALC_ALL_ATTRIBUTES, size, &attrs[0]);
    for (size_t i = 0; i < attrs.size(); ++i){
       if (attrs[i] == ALC_MONO_SOURCES) {
          std::cout << "AL: max mono sources: " << attrs[i+1] << std::endl;
          maxSources = attrs[i+1];
       }
    }
    auto devices = getAvailableDevices();
    std::cout << "AL devices:" << std::endl;
    for (auto& name : devices) {
        std::cout << "  " << name << std::endl;
    }
}

ALAudio::~ALAudio() {
    for (uint source : allsources) {
        int state = AL::getSourcei(source, AL_SOURCE_STATE);
        if (state == AL_PLAYING || state == AL_PAUSED) {
            AL_CHECK(alSourceStop(source));
        }
        AL_CHECK(alDeleteSources(1, &source));
    }

    for (uint buffer : allbuffers){
        AL_CHECK(alDeleteBuffers(1, &buffer));
    }

    AL_CHECK(alcMakeContextCurrent(context));
    alcDestroyContext(context);
    if (!alcCloseDevice(device)) {
        std::cerr << "AL: device not closed!" << std::endl;
    }
    device = nullptr;
    context = nullptr;
}

Sound* ALAudio::createSound(std::shared_ptr<PCM> pcm, bool keepPCM) {
    auto format = AL::to_al_format(pcm->channels, pcm->bitsPerSample);
    uint buffer = getFreeBuffer();
    AL_CHECK(alBufferData(buffer, format, pcm->data.data(), pcm->data.size(), pcm->sampleRate));
    return new ALSound(this, buffer, pcm, keepPCM);
}

ALAudio* ALAudio::create() {
    ALCdevice* device = alcOpenDevice(nullptr);
    if (device == nullptr)
        return nullptr;
    ALCcontext* context = alcCreateContext(device, nullptr);
    if (!alcMakeContextCurrent(context)){
        alcCloseDevice(device);
        return nullptr;
    }
    AL_CHECK();
    std::cout << "AL: initialized" << std::endl;
    return new ALAudio(device, context);
}

uint ALAudio::getFreeSource(){
    if (!freesources.empty()){
        uint source = freesources.back();
        freesources.pop_back();
        return source;
    }
    if (allsources.size() == maxSources){
        std::cerr << "attempted to create new source, but limit is " << maxSources << std::endl;
        return 0;
    }
    ALuint id;
    alGenSources(1, &id);
    if (!AL_GET_ERORR())
        return 0;
    allsources.push_back(id);
    return id;
}

uint ALAudio::getFreeBuffer(){
    if (!freebuffers.empty()){
        uint buffer = freebuffers.back();
        freebuffers.pop_back();
        return buffer;
    }
    if (allbuffers.size() == maxBuffers){
        std::cerr << "attempted to create new ALbuffer, but limit is " << maxBuffers << std::endl;
        return 0;
    }
    ALuint id;
    alGenBuffers(1, &id);
    if (!AL_GET_ERORR()) {
        return 0;
    }

    allbuffers.push_back(id);
    return id;
}

void ALAudio::freeSource(uint source){
    freesources.push_back(source);
}

void ALAudio::freeBuffer(uint buffer){
    freebuffers.push_back(buffer);
}

std::vector<std::string> ALAudio::getAvailableDevices() const {
    std::vector<std::string> devicesVec;

    const ALCchar* devices;
    devices = alcGetString(device, ALC_DEVICE_SPECIFIER);
    if (!AL_GET_ERORR()) {
        return devicesVec;
    }

    const char* ptr = devices;
    do {
        devicesVec.push_back(std::string(ptr));
        ptr += devicesVec.back().size() + 1;
    }
    while (ptr[0]);

    return devicesVec;
}

void ALAudio::setListener(glm::vec3 position, glm::vec3 velocity, glm::vec3 at, glm::vec3 up){
    ALfloat listenerOri[] = { at.x, at.y, at.z, up.x, up.y, up.z };

    AL_CHECK(alListener3f(AL_POSITION, position.x, position.y, position.z));
    AL_CHECK(alListener3f(AL_VELOCITY, velocity.x, velocity.y, velocity.z));
    AL_CHECK(alListenerfv(AL_ORIENTATION, listenerOri));
}

void ALAudio::update(double delta) {
}
