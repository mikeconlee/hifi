//
//  ScriptEngine.cpp
//  hifi
//
//  Created by Brad Hefta-Gaub on 12/14/13.
//  Copyright (c) 2013 HighFidelity, Inc. All rights reserved.
//

#include <QtCore/QCoreApplication>
#include <QtCore/QEventLoop>
#include <QtCore/QTimer>
#include <QtCore/QThread>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>

#include <AudioRingBuffer.h>
#include <AvatarData.h>
#include <NodeList.h>
#include <PacketHeaders.h>
#include <UUID.h>
#include <VoxelConstants.h>
#include <VoxelDetail.h>
#include <ParticlesScriptingInterface.h>

#include <Sound.h>

#include "MenuItemProperties.h"
#include "LocalVoxels.h"
#include "ScriptEngine.h"

VoxelsScriptingInterface ScriptEngine::_voxelsScriptingInterface;
ParticlesScriptingInterface ScriptEngine::_particlesScriptingInterface;

static QScriptValue soundConstructor(QScriptContext* context, QScriptEngine* engine) {
    QUrl soundURL = QUrl(context->argument(0).toString());
    QScriptValue soundScriptValue = engine->newQObject(new Sound(soundURL), QScriptEngine::ScriptOwnership);

    return soundScriptValue;
}


ScriptEngine::ScriptEngine(const QString& scriptContents, const QString& fileNameString,
                           AbstractControllerScriptingInterface* controllerScriptingInterface) :

    _scriptContents(scriptContents),
    _isFinished(false),
    _isRunning(false),
    _isInitialized(false),
    _engine(),
    _isAvatar(false),
    _avatarIdentityTimer(NULL),
    _avatarBillboardTimer(NULL),
    _timerFunctionMap(),
    _isListeningToAudioStream(false),
    _avatarSound(NULL),
    _numAvatarSoundSentBytes(0),
    _controllerScriptingInterface(controllerScriptingInterface),
    _avatarData(NULL),
    _scriptName(),
    _fileNameString(fileNameString),
    _quatLibrary(),
    _vec3Library()
{
}

ScriptEngine::ScriptEngine(const QUrl& scriptURL,
                           AbstractControllerScriptingInterface* controllerScriptingInterface)  :
    _scriptContents(),
    _isFinished(false),
    _isRunning(false),
    _isInitialized(false),
    _engine(),
    _isAvatar(false),
    _avatarIdentityTimer(NULL),
    _avatarBillboardTimer(NULL),
    _timerFunctionMap(),
    _isListeningToAudioStream(false),
    _avatarSound(NULL),
    _numAvatarSoundSentBytes(0),
    _controllerScriptingInterface(controllerScriptingInterface),
    _avatarData(NULL),
    _scriptName(),
    _fileNameString(),
    _quatLibrary(),
    _vec3Library()
{
    QString scriptURLString = scriptURL.toString();
    _fileNameString = scriptURLString;

    QUrl url(scriptURL);

    // if the scheme is empty, maybe they typed in a file, let's try
    if (url.scheme().isEmpty()) {
        url = QUrl::fromLocalFile(scriptURLString);
    }

    // ok, let's see if it's valid... and if so, load it
    if (url.isValid()) {
        if (url.scheme() == "file") {
            QString fileName = url.toLocalFile();
            QFile scriptFile(fileName);
            if (scriptFile.open(QFile::ReadOnly | QFile::Text)) {
                qDebug() << "Loading file:" << fileName;
                QTextStream in(&scriptFile);
                _scriptContents = in.readAll();
            } else {
                qDebug() << "ERROR Loading file:" << fileName;
            }
        } else {
            QNetworkAccessManager* networkManager = new QNetworkAccessManager(this);
            QNetworkReply* reply = networkManager->get(QNetworkRequest(url));
            qDebug() << "Downloading included script at" << url;
            QEventLoop loop;
            QObject::connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
            loop.exec();
            _scriptContents = reply->readAll();
        }
    }
}

void ScriptEngine::setIsAvatar(bool isAvatar) {
    _isAvatar = isAvatar;

    if (_isAvatar && !_avatarIdentityTimer) {
        // set up the avatar timers
        _avatarIdentityTimer = new QTimer(this);
        _avatarBillboardTimer = new QTimer(this);

        // connect our slot
        connect(_avatarIdentityTimer, &QTimer::timeout, this, &ScriptEngine::sendAvatarIdentityPacket);
        connect(_avatarBillboardTimer, &QTimer::timeout, this, &ScriptEngine::sendAvatarBillboardPacket);

        // start the timers
        _avatarIdentityTimer->start(AVATAR_IDENTITY_PACKET_SEND_INTERVAL_MSECS);
        _avatarBillboardTimer->start(AVATAR_BILLBOARD_PACKET_SEND_INTERVAL_MSECS);
    }
}

void ScriptEngine::setAvatarData(AvatarData* avatarData, const QString& objectName) {
    _avatarData = avatarData;

    // remove the old Avatar property, if it exists
    _engine.globalObject().setProperty(objectName, QScriptValue());

    // give the script engine the new Avatar script property
    registerGlobalObject(objectName, _avatarData);
}

bool ScriptEngine::setScriptContents(const QString& scriptContents, const QString& fileNameString) {
    if (_isRunning) {
        return false;
    }
    _scriptContents = scriptContents;
    _fileNameString = fileNameString;
    return true;
}

Q_SCRIPT_DECLARE_QMETAOBJECT(AudioInjectorOptions, QObject*)
Q_SCRIPT_DECLARE_QMETAOBJECT(LocalVoxels, QString)

void ScriptEngine::init() {
    if (_isInitialized) {
        return; // only initialize once
    }

    _isInitialized = true;

    _voxelsScriptingInterface.init();
    _particlesScriptingInterface.init();

    // register various meta-types
    registerMetaTypes(&_engine);
    registerVoxelMetaTypes(&_engine);
    registerEventTypes(&_engine);
    registerMenuItemProperties(&_engine);

    qScriptRegisterMetaType(&_engine, ParticlePropertiesToScriptValue, ParticlePropertiesFromScriptValue);
    qScriptRegisterMetaType(&_engine, ParticleIDtoScriptValue, ParticleIDfromScriptValue);
    qScriptRegisterSequenceMetaType<QVector<ParticleID> >(&_engine);
    qScriptRegisterSequenceMetaType<QVector<glm::vec2> >(&_engine);
    qScriptRegisterSequenceMetaType<QVector<QString> >(&_engine);

    QScriptValue soundConstructorValue = _engine.newFunction(soundConstructor);
    QScriptValue soundMetaObject = _engine.newQMetaObject(&Sound::staticMetaObject, soundConstructorValue);
    _engine.globalObject().setProperty("Sound", soundMetaObject);

    QScriptValue injectionOptionValue = _engine.scriptValueFromQMetaObject<AudioInjectorOptions>();
    _engine.globalObject().setProperty("AudioInjectionOptions", injectionOptionValue);

    QScriptValue localVoxelsValue = _engine.scriptValueFromQMetaObject<LocalVoxels>();
    _engine.globalObject().setProperty("LocalVoxels", localVoxelsValue);

    registerGlobalObject("Script", this);
    registerGlobalObject("Audio", &_audioScriptingInterface);
    registerGlobalObject("Controller", _controllerScriptingInterface);
    registerGlobalObject("Particles", &_particlesScriptingInterface);
    registerGlobalObject("Quat", &_quatLibrary);
    registerGlobalObject("Vec3", &_vec3Library);

    registerGlobalObject("Voxels", &_voxelsScriptingInterface);

    QScriptValue treeScaleValue = _engine.newVariant(QVariant(TREE_SCALE));
    _engine.globalObject().setProperty("TREE_SCALE", treeScaleValue);

    // let the VoxelPacketSender know how frequently we plan to call it
    _voxelsScriptingInterface.getVoxelPacketSender()->setProcessCallIntervalHint(SCRIPT_DATA_CALLBACK_USECS);
    _particlesScriptingInterface.getParticlePacketSender()->setProcessCallIntervalHint(SCRIPT_DATA_CALLBACK_USECS);
}

void ScriptEngine::registerGlobalObject(const QString& name, QObject* object) {
    if (object) {
        QScriptValue value = _engine.newQObject(object);
        _engine.globalObject().setProperty(name, value);
    }
}

void ScriptEngine::evaluate() {
    if (!_isInitialized) {
        init();
    }

    QScriptValue result = _engine.evaluate(_scriptContents);

    if (_engine.hasUncaughtException()) {
        int line = _engine.uncaughtExceptionLineNumber();
        qDebug() << "Uncaught exception at line" << line << ":" << result.toString();
    }
}

void ScriptEngine::sendAvatarIdentityPacket() {
    if (_isAvatar && _avatarData) {
        _avatarData->sendIdentityPacket();
    }
}

void ScriptEngine::sendAvatarBillboardPacket() {
    if (_isAvatar && _avatarData) {
        _avatarData->sendBillboardPacket();
    }
}

void ScriptEngine::run() {
    if (!_isInitialized) {
        init();
    }
    _isRunning = true;

    QScriptValue result = _engine.evaluate(_scriptContents);
    if (_engine.hasUncaughtException()) {
        int line = _engine.uncaughtExceptionLineNumber();
        qDebug() << "Uncaught exception at line" << line << ":" << result.toString();
    }

    timeval startTime;
    gettimeofday(&startTime, NULL);

    int thisFrame = 0;

    NodeList* nodeList = NodeList::getInstance();

    qint64 lastUpdate = usecTimestampNow();

    while (!_isFinished) {
        int usecToSleep = usecTimestamp(&startTime) + (thisFrame++ * SCRIPT_DATA_CALLBACK_USECS) - usecTimestampNow();
        if (usecToSleep > 0) {
            usleep(usecToSleep);
        }

        if (_isFinished) {
            break;
        }

        QCoreApplication::processEvents();

        if (_isFinished) {
            break;
        }

        if (_voxelsScriptingInterface.getVoxelPacketSender()->serversExist()) {
            // release the queue of edit voxel messages.
            _voxelsScriptingInterface.getVoxelPacketSender()->releaseQueuedMessages();

            // since we're in non-threaded mode, call process so that the packets are sent
            if (!_voxelsScriptingInterface.getVoxelPacketSender()->isThreaded()) {
                _voxelsScriptingInterface.getVoxelPacketSender()->process();
            }
        }

        if (_particlesScriptingInterface.getParticlePacketSender()->serversExist()) {
            // release the queue of edit voxel messages.
            _particlesScriptingInterface.getParticlePacketSender()->releaseQueuedMessages();

            // since we're in non-threaded mode, call process so that the packets are sent
            if (!_particlesScriptingInterface.getParticlePacketSender()->isThreaded()) {
                _particlesScriptingInterface.getParticlePacketSender()->process();
            }
        }

        if (_isAvatar && _avatarData) {

            const int SCRIPT_AUDIO_BUFFER_SAMPLES = floor(((SCRIPT_DATA_CALLBACK_USECS * SAMPLE_RATE) / (1000 * 1000)) + 0.5);
            const int SCRIPT_AUDIO_BUFFER_BYTES = SCRIPT_AUDIO_BUFFER_SAMPLES * sizeof(int16_t);

            QByteArray avatarPacket = byteArrayWithPopulatedHeader(PacketTypeAvatarData);
            avatarPacket.append(_avatarData->toByteArray());

            nodeList->broadcastToNodes(avatarPacket, NodeSet() << NodeType::AvatarMixer);

            if (_isListeningToAudioStream || _avatarSound) {
                // if we have an avatar audio stream then send it out to our audio-mixer
                bool silentFrame = true;

                int16_t numAvailableSamples = SCRIPT_AUDIO_BUFFER_SAMPLES;
                const int16_t* nextSoundOutput = NULL;

                if (_avatarSound) {

                    const QByteArray& soundByteArray = _avatarSound->getByteArray();
                    nextSoundOutput = reinterpret_cast<const int16_t*>(soundByteArray.data()
                                                                       + _numAvatarSoundSentBytes);

                    int numAvailableBytes = (soundByteArray.size() - _numAvatarSoundSentBytes) > SCRIPT_AUDIO_BUFFER_BYTES
                        ? SCRIPT_AUDIO_BUFFER_BYTES
                        : soundByteArray.size() - _numAvatarSoundSentBytes;
                    numAvailableSamples = numAvailableBytes / sizeof(int16_t);


                    // check if the all of the _numAvatarAudioBufferSamples to be sent are silence
                    for (int i = 0; i < numAvailableSamples; ++i) {
                        if (nextSoundOutput[i] != 0) {
                            silentFrame = false;
                            break;
                        }
                    }

                    _numAvatarSoundSentBytes += numAvailableBytes;
                    if (_numAvatarSoundSentBytes == soundByteArray.size()) {
                        // we're done with this sound object - so set our pointer back to NULL
                        // and our sent bytes back to zero
                        _avatarSound = NULL;
                        _numAvatarSoundSentBytes = 0;
                    }
                }

                QByteArray audioPacket = byteArrayWithPopulatedHeader(silentFrame
                                                                      ? PacketTypeSilentAudioFrame
                                                                      : PacketTypeMicrophoneAudioNoEcho);

                QDataStream packetStream(&audioPacket, QIODevice::Append);

                // use the orientation and position of this avatar for the source of this audio
                packetStream.writeRawData(reinterpret_cast<const char*>(&_avatarData->getPosition()), sizeof(glm::vec3));
                glm::quat headOrientation = _avatarData->getHeadOrientation();
                packetStream.writeRawData(reinterpret_cast<const char*>(&headOrientation), sizeof(glm::quat));

                if (silentFrame) {
                    if (!_isListeningToAudioStream) {
                        // if we have a silent frame and we're not listening then just send nothing and break out of here
                        break;
                    }

                    // write the number of silent samples so the audio-mixer can uphold timing
                    packetStream.writeRawData(reinterpret_cast<const char*>(&SCRIPT_AUDIO_BUFFER_SAMPLES), sizeof(int16_t));
                } else if (nextSoundOutput) {
                    // write the raw audio data
                    packetStream.writeRawData(reinterpret_cast<const char*>(nextSoundOutput),
                                              numAvailableSamples * sizeof(int16_t));
                }

                nodeList->broadcastToNodes(audioPacket, NodeSet() << NodeType::AudioMixer);
            }
        }

        qint64 now = usecTimestampNow();
        float deltaTime = (float) (now - lastUpdate) / (float) USECS_PER_SECOND;
        emit update(deltaTime);
        lastUpdate = now;

        if (_engine.hasUncaughtException()) {
            int line = _engine.uncaughtExceptionLineNumber();
            qDebug() << "Uncaught exception at line" << line << ":" << _engine.uncaughtException().toString();
        }
    }
    emit scriptEnding();

    // kill the avatar identity timer
    delete _avatarIdentityTimer;

    if (_voxelsScriptingInterface.getVoxelPacketSender()->serversExist()) {
        // release the queue of edit voxel messages.
        _voxelsScriptingInterface.getVoxelPacketSender()->releaseQueuedMessages();

        // since we're in non-threaded mode, call process so that the packets are sent
        if (!_voxelsScriptingInterface.getVoxelPacketSender()->isThreaded()) {
            _voxelsScriptingInterface.getVoxelPacketSender()->process();
        }
    }

    if (_particlesScriptingInterface.getParticlePacketSender()->serversExist()) {
        // release the queue of edit voxel messages.
        _particlesScriptingInterface.getParticlePacketSender()->releaseQueuedMessages();

        // since we're in non-threaded mode, call process so that the packets are sent
        if (!_particlesScriptingInterface.getParticlePacketSender()->isThreaded()) {
            _particlesScriptingInterface.getParticlePacketSender()->process();
        }
    }

    // If we were on a thread, then wait till it's done
    if (thread()) {
        thread()->quit();
    }

    emit finished(_fileNameString);

    _isRunning = false;
}

void ScriptEngine::stop() {
    _isFinished = true;
}

void ScriptEngine::timerFired() {
    QTimer* callingTimer = reinterpret_cast<QTimer*>(sender());

    // call the associated JS function, if it exists
    QScriptValue timerFunction = _timerFunctionMap.value(callingTimer);
    if (timerFunction.isValid()) {
        timerFunction.call();
    }

    if (!callingTimer->isActive()) {
        // this timer is done, we can kill it
        delete callingTimer;
    }
}

QObject* ScriptEngine::setupTimerWithInterval(const QScriptValue& function, int intervalMS, bool isSingleShot) {
    // create the timer, add it to the map, and start it
    QTimer* newTimer = new QTimer(this);
    newTimer->setSingleShot(isSingleShot);

    connect(newTimer, &QTimer::timeout, this, &ScriptEngine::timerFired);

    // make sure the timer stops when the script does
    connect(this, &ScriptEngine::scriptEnding, newTimer, &QTimer::stop);

    _timerFunctionMap.insert(newTimer, function);

    newTimer->start(intervalMS);
    return newTimer;
}

QObject* ScriptEngine::setInterval(const QScriptValue& function, int intervalMS) {
    return setupTimerWithInterval(function, intervalMS, false);
}

QObject* ScriptEngine::setTimeout(const QScriptValue& function, int timeoutMS) {
    return setupTimerWithInterval(function, timeoutMS, true);
}

void ScriptEngine::stopTimer(QTimer *timer) {
    if (_timerFunctionMap.contains(timer)) {
        timer->stop();
        _timerFunctionMap.remove(timer);
        delete timer;
    }
}

QUrl ScriptEngine::resolveInclude(const QString& include) const {
    // first lets check to see if it's already a full URL
    QUrl url(include);
    if (!url.scheme().isEmpty()) {
        return url;
    }

    // we apparently weren't a fully qualified url, so, let's assume we're relative
    // to the original URL of our script
    QUrl parentURL(_fileNameString);

    // if the parent URL's scheme is empty, then this is probably a local file...
    if (parentURL.scheme().isEmpty()) {
        parentURL = QUrl::fromLocalFile(_fileNameString);
    }

    // at this point we should have a legitimate fully qualified URL for our parent
    url = parentURL.resolved(url);
    return url;
}

void ScriptEngine::include(const QString& includeFile) {
    QUrl url = resolveInclude(includeFile);
    QString includeContents;

    if (url.scheme() == "file") {
        QString fileName = url.toLocalFile();
        QFile scriptFile(fileName);
        if (scriptFile.open(QFile::ReadOnly | QFile::Text)) {
            qDebug() << "Loading file:" << fileName;
            QTextStream in(&scriptFile);
            includeContents = in.readAll();
        } else {
            qDebug() << "ERROR Loading file:" << fileName;
        }
    } else {
        QNetworkAccessManager* networkManager = new QNetworkAccessManager(this);
        QNetworkReply* reply = networkManager->get(QNetworkRequest(url));
        qDebug() << "Downloading included script at" << includeFile;
        QEventLoop loop;
        QObject::connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
        loop.exec();
        includeContents = reply->readAll();
    }

    QScriptValue result = _engine.evaluate(includeContents);
    if (_engine.hasUncaughtException()) {
        int line = _engine.uncaughtExceptionLineNumber();
        qDebug() << "Uncaught exception at (" << includeFile << ") line" << line << ":" << result.toString();
    }
}
