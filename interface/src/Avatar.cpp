//
//  Avatar.cpp
//  interface
//
//  Created by Philip Rosedale on 9/11/12.
//	adapted by Jeffrey Ventrella
//  Copyright (c) 2012 Physical, Inc.. All rights reserved.
//

#include <glm/glm.hpp>
#include <vector>
#include <lodepng.h>
#include <SharedUtil.h>
#include "Avatar.h"
#include "Log.h"
#include "ui/TextRenderer.h"
#include <AgentList.h>
#include <AgentTypes.h>
#include <PacketHeaders.h>

using namespace std;

const bool BALLS_ON = false;

const bool  AVATAR_GRAVITY          = true;
const float DECAY                   = 0.1;
const float THRUST_MAG              = 1200.0;
const float YAW_MAG                 = 500.0; //JJV - changed from 300.0;
const float TEST_YAW_DECAY          = 5.0;
const float LIN_VEL_DECAY           = 5.0;
const float MY_HAND_HOLDING_PULL    = 0.2;
const float YOUR_HAND_HOLDING_PULL  = 1.0;
const float BODY_SPRING_FORCE       = 6.0f;
const float BODY_SPRING_DECAY       = 16.0f;
//const float COLLISION_FRICTION      = 0.5;
//const float COLLISION_RADIUS_SCALAR = 1.8;
//const float COLLISION_BALL_FORCE    = 0.1;
//const float COLLISION_BODY_FORCE    = 3.0;

const float COLLISION_RADIUS_SCALAR = 1.8;
const float COLLISION_BALL_FORCE    = 0.6;
const float COLLISION_BODY_FORCE    = 6.0;
const float COLLISION_BALL_FRICTION = 200.0;
const float COLLISION_BODY_FRICTION = 0.5;
    
float skinColor[] = {1.0, 0.84, 0.66};
float lightBlue[] = { 0.7, 0.8, 1.0 };
float browColor[] = {210.0/255.0, 105.0/255.0, 30.0/255.0};
float mouthColor[] = {1, 0, 0};

float BrowRollAngle[5] = {0, 15, 30, -30, -15};
float BrowPitchAngle[3] = {-70, -60, -50};
float eyeColor[3] = {1,1,1};

float MouthWidthChoices[3] = {0.5, 0.77, 0.3};

float browWidth = 0.8;
float browThickness = 0.16;

bool usingBigSphereCollisionTest = true;

char iris_texture_file[] = "resources/images/green_eye.png";

float chatMessageScale = 0.001;
float chatMessageHeight = 0.4;

vector<unsigned char> iris_texture;
unsigned int iris_texture_width = 512;
unsigned int iris_texture_height = 256;

Avatar::Avatar(bool isMine) {
    
    _orientation.setToIdentity();
    
	_velocity                   = glm::vec3( 0.0, 0.0, 0.0 );
	_thrust                     = glm::vec3( 0.0, 0.0, 0.0 );
    _rotation                   = glm::quat( 0.0f, 0.0f, 0.0f, 0.0f );
	_bodyYaw                    = -90.0;
	_bodyPitch                  = 0.0;
	_bodyRoll                   = 0.0;
	_bodyYawDelta               = 0.0;
	_mousePressed               = false;
	_mode                       = AVATAR_MODE_STANDING;
    _isMine                     = isMine;
    _maxArmLength               = 0.0;
    _transmitterHz              = 0.0;
    _transmitterPackets         = 0;
    _transmitterIsFirstData     = true;
    _transmitterInitialReading  = glm::vec3( 0.f, 0.f, 0.f );
    _speed                      = 0.0;
    _pelvisStandingHeight       = 0.0f;
    _displayingHead             = true;
    _TEST_bigSphereRadius       = 0.3f;
    _TEST_bigSpherePosition     = glm::vec3( 0.0f, _TEST_bigSphereRadius, 2.0f );
    
    for (int i = 0; i < MAX_DRIVE_KEYS; i++) _driveKeys[i] = false;
    
    _head.pupilSize             = 0.10;
    _head.interPupilDistance    = 0.6;
    _head.interBrowDistance     = 0.75;
    _head.nominalPupilSize      = 0.10;
    _head.pitchRate             = 0.0;
    _head.yawRate               = 0.0;
    _head.rollRate              = 0.0;
    _head.eyebrowPitch[0]       = -30;
    _head.eyebrowPitch[1]       = -30;
    _head.eyebrowRoll [0]       = 20;
    _head.eyebrowRoll [1]       = -20;
    _head.mouthPitch            = 0;
    _head.mouthYaw              = 0;
    _head.mouthWidth            = 1.0;
    _head.mouthHeight           = 0.2;
    _head.eyeballPitch[0]       = 0;
    _head.eyeballPitch[1]       = 0;
    _head.eyeballScaleX         = 1.2;
    _head.eyeballScaleY         = 1.5;
    _head.eyeballScaleZ         = 1.0;
    _head.eyeballYaw[0]         = 0;
    _head.eyeballYaw[1]         = 0;
    _head.pitchTarget           = 0;
    _head.yawTarget             = 0;
    _head.noiseEnvelope         = 1.0;
    _head.pupilConverge         = 10.0;
    _head.leanForward           = 0.0;
    _head.leanSideways          = 0.0;
    _head.eyeContact            = 1;
    _head.eyeContactTarget      = LEFT_EYE;
    _head.scale                 = 1.0;
    _head.audioAttack           = 0.0;
    _head.averageLoudness       = 0.0;
    _head.lastLoudness          = 0.0;
    _head.browAudioLift         = 0.0;
    _head.noise                 = 0;
    _head.returnSpringScale     = 1.0;
	_movedHandOffset            = glm::vec3( 0.0, 0.0, 0.0 );
    _usingBodySprings           = true;
    _renderYaw                  = 0.0;
    _renderPitch                = 0.0;
	_sphere                     = NULL;
    _interactingOther           = NULL;
	_interactingOtherIsNearby   = false;
    _handHoldingPosition        = glm::vec3( 0.0, 0.0, 0.0 );
    
    initializeSkeleton();
    
    if (iris_texture.size() == 0) {
        switchToResourcesParentIfRequired();
        unsigned error = lodepng::decode(iris_texture, iris_texture_width, iris_texture_height, iris_texture_file);
        if (error != 0) {
            printLog("error %u: %s\n", error, lodepng_error_text(error));
        }
    }
    
    if (BALLS_ON)   { _balls = new Balls(100); }
    else            { _balls = NULL; }
}


Avatar::Avatar(const Avatar &otherAvatar) {
    
    _velocity                       = otherAvatar._velocity;
	_thrust                         = otherAvatar._thrust;
    _rotation                       = otherAvatar._rotation;
	_interactingOtherIsNearby       = otherAvatar._interactingOtherIsNearby;
	_bodyYaw                        = otherAvatar._bodyYaw;
	_bodyPitch                      = otherAvatar._bodyPitch;
	_bodyRoll                       = otherAvatar._bodyRoll;
	_bodyYawDelta                   = otherAvatar._bodyYawDelta;
	_mousePressed                   = otherAvatar._mousePressed;
	_mode                           = otherAvatar._mode;
    _isMine                         = otherAvatar._isMine;
    _renderYaw                      = otherAvatar._renderYaw;
    _renderPitch                    = otherAvatar._renderPitch;
    _maxArmLength                   = otherAvatar._maxArmLength;
    _transmitterTimer               = otherAvatar._transmitterTimer;
    _transmitterIsFirstData         = otherAvatar._transmitterIsFirstData;
    _transmitterTimeLastReceived    = otherAvatar._transmitterTimeLastReceived;
    _transmitterHz                  = otherAvatar._transmitterHz;
    _transmitterInitialReading      = otherAvatar._transmitterInitialReading;
    _transmitterPackets             = otherAvatar._transmitterPackets;
    _TEST_bigSphereRadius           = otherAvatar._TEST_bigSphereRadius;
    _TEST_bigSpherePosition         = otherAvatar._TEST_bigSpherePosition;
	_movedHandOffset                = otherAvatar._movedHandOffset;
	_usingBodySprings               = otherAvatar._usingBodySprings;
	_orientation.set( otherAvatar._orientation );
    
	_sphere = NULL;
    
    initializeSkeleton();
    
    for (int i = 0; i < MAX_DRIVE_KEYS; i++) _driveKeys[i] = otherAvatar._driveKeys[i];
    
    _head.pupilSize          = otherAvatar._head.pupilSize;
    _head.interPupilDistance = otherAvatar._head.interPupilDistance;
    _head.interBrowDistance  = otherAvatar._head.interBrowDistance;
    _head.nominalPupilSize   = otherAvatar._head.nominalPupilSize;
    _head.yawRate            = otherAvatar._head.yawRate;
    _head.pitchRate          = otherAvatar._head.pitchRate;
    _head.rollRate           = otherAvatar._head.rollRate;
    _head.eyebrowPitch[0]    = otherAvatar._head.eyebrowPitch[0];
    _head.eyebrowPitch[1]    = otherAvatar._head.eyebrowPitch[1];
    _head.eyebrowRoll [0]    = otherAvatar._head.eyebrowRoll [0];
    _head.eyebrowRoll [1]    = otherAvatar._head.eyebrowRoll [1];
    _head.mouthPitch         = otherAvatar._head.mouthPitch;
    _head.mouthYaw           = otherAvatar._head.mouthYaw;
    _head.mouthWidth         = otherAvatar._head.mouthWidth;
    _head.mouthHeight        = otherAvatar._head.mouthHeight;
    _head.eyeballPitch[0]    = otherAvatar._head.eyeballPitch[0];
    _head.eyeballPitch[1]    = otherAvatar._head.eyeballPitch[1];
    _head.eyeballScaleX      = otherAvatar._head.eyeballScaleX;
    _head.eyeballScaleY      = otherAvatar._head.eyeballScaleY;
    _head.eyeballScaleZ      = otherAvatar._head.eyeballScaleZ;
    _head.eyeballYaw[0]      = otherAvatar._head.eyeballYaw[0];
    _head.eyeballYaw[1]      = otherAvatar._head.eyeballYaw[1];
    _head.pitchTarget        = otherAvatar._head.pitchTarget;
    _head.yawTarget          = otherAvatar._head.yawTarget;
    _head.noiseEnvelope      = otherAvatar._head.noiseEnvelope;
    _head.pupilConverge      = otherAvatar._head.pupilConverge;
    _head.leanForward        = otherAvatar._head.leanForward;
    _head.leanSideways       = otherAvatar._head.leanSideways;
    _head.eyeContact         = otherAvatar._head.eyeContact;
    _head.eyeContactTarget   = otherAvatar._head.eyeContactTarget;
    _head.scale              = otherAvatar._head.scale;
    _head.audioAttack        = otherAvatar._head.audioAttack;
    _head.averageLoudness    = otherAvatar._head.averageLoudness;
    _head.lastLoudness       = otherAvatar._head.lastLoudness;
    _head.browAudioLift      = otherAvatar._head.browAudioLift;
    _head.noise              = otherAvatar._head.noise;
    

    initializeSkeleton();
    
    if (iris_texture.size() == 0) {
        switchToResourcesParentIfRequired();
        unsigned error = lodepng::decode(iris_texture, iris_texture_width, iris_texture_height, iris_texture_file);
        if (error != 0) {
            printLog("error %u: %s\n", error, lodepng_error_text(error));
        }
    }
}

Avatar::~Avatar()  {
    if (_sphere != NULL) {
        gluDeleteQuadric(_sphere);
    }
}

Avatar* Avatar::clone() const {
    return new Avatar(*this);
}

void Avatar::reset() {
    _headPitch = _headYaw = _headRoll = 0;
    _head.leanForward = _head.leanSideways = 0;
}


//this pertains to moving the head with the glasses
void Avatar::UpdateGyros(float frametime, SerialInterface * serialInterface, glm::vec3 * gravity)
//  Using serial data, update avatar/render position and angles
{
    const float PITCH_ACCEL_COUPLING = 0.5;
    const float ROLL_ACCEL_COUPLING = -1.0;
    float measured_pitch_rate = serialInterface->getRelativeValue(HEAD_PITCH_RATE);
    _head.yawRate = serialInterface->getRelativeValue(HEAD_YAW_RATE);
    float measured_lateral_accel = serialInterface->getRelativeValue(ACCEL_X) -
    ROLL_ACCEL_COUPLING * serialInterface->getRelativeValue(HEAD_ROLL_RATE);
    float measured_fwd_accel = serialInterface->getRelativeValue(ACCEL_Z) -
    PITCH_ACCEL_COUPLING * serialInterface->getRelativeValue(HEAD_PITCH_RATE);
    float measured_roll_rate = serialInterface->getRelativeValue(HEAD_ROLL_RATE);
    
    //printLog("Pitch Rate: %d ACCEL_Z: %d\n", serialInterface->getRelativeValue(PITCH_RATE),
    //                                         serialInterface->getRelativeValue(ACCEL_Z));
    //printLog("Pitch Rate: %d ACCEL_X: %d\n", serialInterface->getRelativeValue(PITCH_RATE),
    //                                         serialInterface->getRelativeValue(ACCEL_Z));
    //printLog("Pitch: %f\n", Pitch);
    
    //  Update avatar head position based on measured gyro rates
    const float HEAD_ROTATION_SCALE = 0.70;
    const float HEAD_ROLL_SCALE = 0.40;
    const float HEAD_LEAN_SCALE = 0.01;
    const float MAX_PITCH = 45;
    const float MIN_PITCH = -45;
    const float MAX_YAW = 85;
    const float MIN_YAW = -85;
    
    if ((_headPitch < MAX_PITCH) && (_headPitch > MIN_PITCH))
        addHeadPitch(measured_pitch_rate * -HEAD_ROTATION_SCALE * frametime);
    
    addHeadRoll(measured_roll_rate * HEAD_ROLL_SCALE * frametime);
    
    if ((_headYaw < MAX_YAW) && (_headYaw > MIN_YAW))
        addHeadYaw(_head.yawRate * HEAD_ROTATION_SCALE * frametime);
    
    addLean(-measured_lateral_accel * frametime * HEAD_LEAN_SCALE, -measured_fwd_accel*frametime * HEAD_LEAN_SCALE);
}

float Avatar::getAbsoluteHeadYaw() const {
    return _bodyYaw + _headYaw;
}

void Avatar::addLean(float x, float z) {
    //  Add Body lean as impulse
    _head.leanSideways += x;
    _head.leanForward  += z;
}

void Avatar::setLeanForward(float dist){
    _head.leanForward = dist;
}

void Avatar::setLeanSideways(float dist){
    _head.leanSideways = dist;
}

void Avatar::setMousePressed( bool d ) {
	_mousePressed = d;
}


void Avatar::simulate(float deltaTime) {
    
    // update balls
    if (_balls) { _balls->simulate(deltaTime); }
    
	// update avatar skeleton
	updateSkeleton();
	
	// reset hand and arm positions according to hand movement
	updateHandMovement( deltaTime );
    
    if ( !_interactingOtherIsNearby ) {
        //initialize _handHolding
        _handHoldingPosition = _bone[ AVATAR_BONE_RIGHT_HAND ].position;
    }
    
    _interactingOtherIsNearby = false;

    // if the avatar being simulated is mine, then loop through
    // all the other avatars for potential interactions...
    if ( _isMine )
    {    
        float closestDistance = 10000.0f;
        
        AgentList* agentList = AgentList::getInstance();
        for (AgentList::iterator agent = agentList->begin(); agent != agentList->end(); agent++) {
            if (agent->getLinkedData() != NULL && agent->getType() == AGENT_TYPE_AVATAR) {
                Avatar *otherAvatar = (Avatar *)agent->getLinkedData();
                
                // check for collisions with other avatars and respond
                updateCollisionWithOtherAvatar(otherAvatar, deltaTime );
                 
                // test other avatar hand position for proximity
                glm::vec3 v( _bone[ AVATAR_BONE_RIGHT_SHOULDER ].position );
                v -= otherAvatar->getBonePosition( AVATAR_BONE_RIGHT_HAND );
                
                float distance = glm::length( v );
                if ( distance < _maxArmLength + _maxArmLength ) {
                                
                    closestDistance = distance;
                    _interactingOther = otherAvatar;
                    _interactingOtherIsNearby = true;
                    
                    // if I am holding hands with another avatar, a force is applied
                    if (( _handState == 1 ) ||  ( _interactingOther->_handState == 1 )) {
                        glm::vec3 vectorToOtherHand = _interactingOther->_handPosition - _handHoldingPosition;
                        glm::vec3 vectorToMyHand = _bone[ AVATAR_BONE_RIGHT_HAND ].position - _handHoldingPosition;
                        _handHoldingPosition += vectorToOtherHand * YOUR_HAND_HOLDING_PULL;
                        _handHoldingPosition += vectorToMyHand    * MY_HAND_HOLDING_PULL;
                        _bone[ AVATAR_BONE_RIGHT_HAND ].position = _handHoldingPosition;
                    }

                    _avatarTouch.setYourHandPosition( _interactingOther->_handPosition );
                }
            }
        }
        
        //  Set the vector we send for hand position to other people to be our right hand
        setHandPosition(_bone[ AVATAR_BONE_RIGHT_HAND ].position);
        
        //update the effects of touching another avatar
        _avatarTouch.simulate(deltaTime);
        
    }//if ( _isMine )
    
    //constrain right arm length and re-adjust elbow position as it bends
    updateArmIKAndConstraints( deltaTime );
    
    if (_isMine) {
        _avatarTouch.setMyHandPosition( _bone[ AVATAR_BONE_RIGHT_HAND ].position );
    }
    
    if (!_interactingOtherIsNearby) {
        _interactingOther = NULL;
    }
    
    if (usingBigSphereCollisionTest) {
        // test for avatar collision response with the big sphere
        updateCollisionWithSphere( _TEST_bigSpherePosition, _TEST_bigSphereRadius, deltaTime );
    }
    
    if ( AVATAR_GRAVITY ) {
        if ( _position.y > _pelvisStandingHeight + 0.01 ) {
            _velocity += glm::dvec3(getGravity(getPosition())) * ( 6.0 * deltaTime );
        } else if ( _position.y < _pelvisStandingHeight ) {
            _position.y = _pelvisStandingHeight;
            _velocity.y = 0.0;
        }
    }
    
	// update body springs
    updateBodySprings( deltaTime );
    
    // driving the avatar around should only apply if this is my avatar (as opposed to an avatar being driven remotely)
    if ( _isMine ) {
        
        _thrust = glm::vec3( 0.0, 0.0, 0.0 );
             
        if (_driveKeys[FWD      ]) {_thrust       += THRUST_MAG * deltaTime * _orientation.getFront();}
        if (_driveKeys[BACK     ]) {_thrust       -= THRUST_MAG * deltaTime * _orientation.getFront();}
        if (_driveKeys[RIGHT    ]) {_thrust       += THRUST_MAG * deltaTime * _orientation.getRight();}
        if (_driveKeys[LEFT     ]) {_thrust       -= THRUST_MAG * deltaTime * _orientation.getRight();}
        if (_driveKeys[UP       ]) {_thrust       += THRUST_MAG * deltaTime * _orientation.getUp();}
        if (_driveKeys[DOWN     ]) {_thrust       -= THRUST_MAG * deltaTime * _orientation.getUp();}
        if (_driveKeys[ROT_RIGHT]) {_bodyYawDelta -= YAW_MAG    * deltaTime;}
        if (_driveKeys[ROT_LEFT ]) {_bodyYawDelta += YAW_MAG    * deltaTime;}
	}
        
    // update body yaw by body yaw delta
    if (_isMine) {
        _bodyYaw += _bodyYawDelta * deltaTime;
    }
    
	// decay body yaw delta
    _bodyYawDelta *= (1.0 - TEST_YAW_DECAY * deltaTime);
    
	// add thrust to velocity
	_velocity += _thrust * deltaTime;

    // update position by velocity
    _position += _velocity * deltaTime;

	// decay velocity
    _velocity *= ( 1.0 - LIN_VEL_DECAY * deltaTime );
	
    // update head information
    updateHead(deltaTime);
    
    // calculate speed, and use that to determine walking vs. standing                                
    _speed = glm::length( _velocity );
	float rotationalSpeed = fabs( _bodyYawDelta );

	if ( _speed + rotationalSpeed > 0.2 ) {
		_mode = AVATAR_MODE_WALKING;
	} else {
		_mode = AVATAR_MODE_INTERACTING;
	}
}




void Avatar::updateHead(float deltaTime) {

    //  Decay head back to center if turned on
    if (_returnHeadToCenter) {
        //  Decay back toward center
        _headPitch *= (1.0f - DECAY * _head.returnSpringScale * 2 * deltaTime);
        _headYaw   *= (1.0f - DECAY * _head.returnSpringScale * 2 * deltaTime);
        _headRoll  *= (1.0f - DECAY * _head.returnSpringScale * 2 * deltaTime);
    }
    
    if (_head.noise) {
        //  Move toward new target
        _headPitch += (_head.pitchTarget - _headPitch) * 10 * deltaTime; // (1.f - DECAY*deltaTime)*Pitch + ;
        _headYaw   += (_head.yawTarget   - _headYaw  ) * 10 * deltaTime; //  (1.f - DECAY*deltaTime);
        _headRoll *= 1.f - (DECAY * deltaTime);
    }
    
    
    _head.leanForward  *= (1.f - DECAY * 30 * deltaTime);
    _head.leanSideways *= (1.f - DECAY * 30 * deltaTime);
    
    //  Update where the avatar's eyes are
    //
    //  First, decide if we are making eye contact or not
    if (randFloat() < 0.005) {
        _head.eyeContact = !_head.eyeContact;
        _head.eyeContact = 1;
        if (!_head.eyeContact) {
            //  If we just stopped making eye contact,move the eyes markedly away
            _head.eyeballPitch[0] = _head.eyeballPitch[1] = _head.eyeballPitch[0] + 5.0 + (randFloat() - 0.5) * 10;
            _head.eyeballYaw  [0] = _head.eyeballYaw  [1] = _head.eyeballYaw  [0] + 5.0 + (randFloat() - 0.5) * 5;
        } else {
            //  If now making eye contact, turn head to look right at viewer
            SetNewHeadTarget(0,0);
        }
    }
    
    const float DEGREES_BETWEEN_VIEWER_EYES = 3;
    const float DEGREES_TO_VIEWER_MOUTH = 7;
    
    if (_head.eyeContact) {
        //  Should we pick a new eye contact target?
        if (randFloat() < 0.01) {
            //  Choose where to look next
            if (randFloat() < 0.1) {
                _head.eyeContactTarget = MOUTH;
            } else {
                if (randFloat() < 0.5) _head.eyeContactTarget = LEFT_EYE; else _head.eyeContactTarget = RIGHT_EYE;
            }
        }
        //  Set eyeball pitch and yaw to make contact
        float eye_target_yaw_adjust = 0;
        float eye_target_pitch_adjust = 0;
        if (_head.eyeContactTarget == LEFT_EYE) eye_target_yaw_adjust = DEGREES_BETWEEN_VIEWER_EYES;
        if (_head.eyeContactTarget == RIGHT_EYE) eye_target_yaw_adjust = -DEGREES_BETWEEN_VIEWER_EYES;
        if (_head.eyeContactTarget == MOUTH) eye_target_pitch_adjust = DEGREES_TO_VIEWER_MOUTH;
        
        _head.eyeballPitch[0] = _head.eyeballPitch[1] = -_headPitch + eye_target_pitch_adjust;
        _head.eyeballYaw[0] = _head.eyeballYaw[1] = -_headYaw + eye_target_yaw_adjust;
    }
	
    
    if (_head.noise)
    {
        _headPitch += (randFloat() - 0.5) * 0.2 * _head.noiseEnvelope;
        _headYaw += (randFloat() - 0.5) * 0.3 *_head.noiseEnvelope;
        //PupilSize += (randFloat() - 0.5) * 0.001*NoiseEnvelope;
        
        if (randFloat() < 0.005) _head.mouthWidth = MouthWidthChoices[rand()%3];
        
        if (!_head.eyeContact) {
            if (randFloat() < 0.01)  _head.eyeballPitch[0] = _head.eyeballPitch[1] = (randFloat() - 0.5) * 20;
            if (randFloat() < 0.01)  _head.eyeballYaw[0] = _head.eyeballYaw[1] = (randFloat()- 0.5) * 10;
        }
        
        if ((randFloat() < 0.005) && (fabs(_head.pitchTarget - _headPitch) < 1.0) && (fabs(_head.yawTarget - _headYaw) < 1.0)) {
            SetNewHeadTarget((randFloat()-0.5) * 20.0, (randFloat()-0.5) * 45.0);
        }
        
        if (0) {
            
            //  Pick new target
            _head.pitchTarget = (randFloat() - 0.5) * 45;
            _head.yawTarget = (randFloat() - 0.5) * 22;
        }
        if (randFloat() < 0.01)
        {
            _head.eyebrowPitch[0] = _head.eyebrowPitch[1] = BrowPitchAngle[rand()%3];
            _head.eyebrowRoll [0] = _head.eyebrowRoll[1] = BrowRollAngle[rand()%5];
            _head.eyebrowRoll [1] *=-1;
        }
    }
    
    //  Update audio trailing average for rendering facial animations
    const float AUDIO_AVERAGING_SECS = 0.05;
    _head.averageLoudness = (1.f - deltaTime / AUDIO_AVERAGING_SECS) * _head.averageLoudness +
                            (deltaTime / AUDIO_AVERAGING_SECS) * _audioLoudness;
}





float Avatar::getHeight() {
    return _height;
}


void Avatar::updateCollisionWithSphere( glm::vec3 position, float radius, float deltaTime ) {
    float myBodyApproximateBoundingRadius = 1.0f;
    glm::vec3 vectorFromMyBodyToBigSphere(_position - position);
    bool jointCollision = false;
    
    float distanceToBigSphere = glm::length(vectorFromMyBodyToBigSphere);
    if ( distanceToBigSphere < myBodyApproximateBoundingRadius + radius ) {
        for (int b = 0; b < NUM_AVATAR_BONES; b++) {
            glm::vec3 vectorFromJointToBigSphereCenter(_bone[b].springyPosition - position);
            float distanceToBigSphereCenter = glm::length(vectorFromJointToBigSphereCenter);
            float combinedRadius = _bone[b].radius + radius;
            
            if ( distanceToBigSphereCenter < combinedRadius )  {
                jointCollision = true;
                if (distanceToBigSphereCenter > 0.0) {
                    glm::vec3 directionVector = vectorFromJointToBigSphereCenter / distanceToBigSphereCenter;
                    
                    float penetration = 1.0 - (distanceToBigSphereCenter / combinedRadius);
                    glm::vec3 collisionForce = vectorFromJointToBigSphereCenter * penetration;
                    
                    _bone[b].springyVelocity += collisionForce *  30.0f * deltaTime;
                    _velocity                += collisionForce * 100.0f * deltaTime;
                    _bone[b].springyPosition = position + directionVector * combinedRadius;
                }
            }
        }
    
        if ( jointCollision ) {
            if (!_usingBodySprings) {
                _usingBodySprings = true;
                initializeBodySprings();
            }
        }
    }
}


//detect collisions with other avatars and respond
void Avatar::updateCollisionWithOtherAvatar( Avatar * otherAvatar, float deltaTime ) {

    // check if the bounding spheres of the two avatars are colliding
    glm::vec3 vectorBetweenBoundingSpheres(_position - otherAvatar->_position);
    if ( glm::length(vectorBetweenBoundingSpheres) < _height * ONE_HALF + otherAvatar->_height * ONE_HALF ) {
        
    float bodyMomentum = 1.0f;
    glm::vec3 bodyPushForce = glm::vec3( 0.0, 0.0, 0.0 );
        
        // loop through the bones of each avatar to check for every possible collision
        for (int b=1; b<NUM_AVATAR_BONES; b++) {
            if ( _bone[b].isCollidable ) {

                for (int o=b+1; o<NUM_AVATAR_BONES; o++) {
                    if ( otherAvatar->_bone[o].isCollidable ) {
                    
                        glm::vec3 vectorBetweenJoints(_bone[b].springyPosition - otherAvatar->_bone[o].springyPosition);
                        float distanceBetweenJoints = glm::length(vectorBetweenJoints);
                        
                        if ( distanceBetweenJoints > 0.0 ) { // to avoid divide by zero
                            float combinedRadius = _bone[b].radius + otherAvatar->_bone[o].radius;

                            // check for collision
                            if ( distanceBetweenJoints < combinedRadius * COLLISION_RADIUS_SCALAR)  {
                                glm::vec3 directionVector = vectorBetweenJoints / distanceBetweenJoints;

                                // push balls away from each other and apply friction
                                glm::vec3 ballPushForce = directionVector * COLLISION_BALL_FORCE * deltaTime;
                                                                
                                                                
                                float ballMomentum = COLLISION_BALL_FRICTION * deltaTime;
                                if ( ballMomentum < 0.0 ) { ballMomentum = 0.0;}
                                                                
                                                                
                                             _bone[b].springyVelocity += ballPushForce;
                                otherAvatar->_bone[o].springyVelocity -= ballPushForce;
                                
                                             _bone[b].springyVelocity *= 0.9;
                                otherAvatar->_bone[o].springyVelocity *= 0.9;
                                
                                // accumulate forces and frictions to the velocities of avatar bodies
                                bodyPushForce += directionVector * COLLISION_BODY_FORCE * deltaTime;                                
                                bodyMomentum -= COLLISION_BODY_FRICTION * deltaTime;
                                if ( bodyMomentum < 0.0 ) { bodyMomentum = 0.0;}
                                                                
                            }// check for collision
                        }   // to avoid divide by zero
                    }      // o loop
                }         // collidable
            }            // b loop
        }               // collidable
        
        
        //apply forces and frictions on the bodies of both avatars 
                     _velocity += bodyPushForce;
        otherAvatar->_velocity -= bodyPushForce;
                     _velocity *= bodyMomentum;
        otherAvatar->_velocity *= bodyMomentum;
        
    } // bounding sphere collision
}    //method




void Avatar::setDisplayingHead( bool displayingHead ) {
    _displayingHead = displayingHead;
}


static TextRenderer* textRenderer() {
    static TextRenderer* renderer = new TextRenderer(SANS_FONT_FAMILY, 24);
    return renderer;
}

void Avatar::render(bool lookingInMirror) {
    
    /*
	// show avatar position
    glColor4f( 0.5f, 0.5f, 0.5f, 0.6 );
	glPushMatrix();
    glTranslatef(_position.x, _position.y, _position.z);
    glScalef( 0.03, 0.03, 0.03 );
    glutSolidSphere( 1, 10, 10 );
	glPopMatrix();
    */
    
    if ( usingBigSphereCollisionTest ) {
        
        // show TEST big sphere
        glColor4f( 0.5f, 0.6f, 0.8f, 0.7 );
        glPushMatrix();
        glTranslatef(_TEST_bigSpherePosition.x, _TEST_bigSpherePosition.y, _TEST_bigSpherePosition.z);
        glScalef( _TEST_bigSphereRadius, _TEST_bigSphereRadius, _TEST_bigSphereRadius );
        glutSolidSphere( 1, 20, 20 );
        glPopMatrix();
    }
    
	// render body
	renderBody();
    
	// render head
    if (_displayingHead) {
        renderHead(lookingInMirror);
	}
    
	// if this is my avatar, then render my interactions with the other avatar
    if ( _isMine )
    {
        if ( _interactingOtherIsNearby ) {					
            _avatarTouch.render();
        }
    }
    
    //  Render the balls
    
    if (_balls) {
        glPushMatrix();
        glTranslatef(_position.x, _position.y, _position.z);
        _balls->render();
        glPopMatrix();
    }

    if (!_chatMessage.empty()) {
        int width = 0;
        int lastWidth;
        for (string::iterator it = _chatMessage.begin(); it != _chatMessage.end(); it++) {
            width += (lastWidth = textRenderer()->computeWidth(*it));
        }
        glPushMatrix();
        
        // extract the view direction from the modelview matrix: transform (0, 0, 1) by the
        // transpose of the modelview to get its direction in world space, then use the X/Z
        // components to determine the angle
        float modelview[16];
        glGetFloatv(GL_MODELVIEW_MATRIX, modelview);
        
        glTranslatef(_position.x, _position.y + chatMessageHeight, _position.z);
        glRotatef(atan2(-modelview[2], -modelview[10]) * 180 / PI, 0, 1, 0);
        
        glColor3f(0, 0.8, 0);
        glRotatef(180, 0, 0, 1);
        glScalef(chatMessageScale, chatMessageScale, 1.0f);

        glDisable(GL_LIGHTING);
        if (_keyState == NO_KEY_DOWN) {
            textRenderer()->draw(-width/2, 0, _chatMessage.c_str());
            
        } else {
            // rather than using substr and allocating a new string, just replace the last
            // character with a null, then restore it
            int lastIndex = _chatMessage.size() - 1;
            char lastChar = _chatMessage[lastIndex];
            _chatMessage[lastIndex] = '\0';
            textRenderer()->draw(-width/2, 0, _chatMessage.c_str());
            _chatMessage[lastIndex] = lastChar;
            glColor3f(0, 1, 0);
            textRenderer()->draw(width/2 - lastWidth, 0, _chatMessage.c_str() + lastIndex);                        
        }
        glEnable(GL_LIGHTING);
        
        glPopMatrix();
    }
}

void Avatar::renderHead(bool lookingInMirror) {
    int side = 0;
    
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_RESCALE_NORMAL);
    
	// show head orientation
	//renderOrientationDirections( _bone[ AVATAR_BONE_HEAD ].position, _bone[ AVATAR_BONE_HEAD ].orientation, 0.2f );
    
    glPushMatrix();
    
	if (_usingBodySprings) {
		glTranslatef(_bone[ AVATAR_BONE_HEAD ].springyPosition.x,
                     _bone[ AVATAR_BONE_HEAD ].springyPosition.y,
                     _bone[ AVATAR_BONE_HEAD ].springyPosition.z);
	}
	else {
		glTranslatef(_bone[ AVATAR_BONE_HEAD ].position.x,
                     _bone[ AVATAR_BONE_HEAD ].position.y,
                     _bone[ AVATAR_BONE_HEAD ].position.z);
	}
	
	glScalef( 0.03, 0.03, 0.03 );
    
    if (lookingInMirror) {
        glRotatef(_bodyYaw - _headYaw,   0, 1, 0);
        glRotatef(_bodyPitch + _headPitch, 1, 0, 0);
        glRotatef(_bodyRoll - _headRoll,  0, 0, 1);
    } else {
        glRotatef(_bodyYaw + _headYaw,   0, 1, 0);
        glRotatef(_bodyPitch + _headPitch, 1, 0, 0);
        glRotatef(_bodyRoll + _headRoll,  0, 0, 1);
    }
    
    glScalef(2.0, 2.0, 2.0);
    glColor3fv(skinColor);
    
    glutSolidSphere(1, 30, 30);
    
    //  Ears
    glPushMatrix();
    glTranslatef(1.0, 0, 0);
    for(side = 0; side < 2; side++) {
        glPushMatrix();
        glScalef(0.3, 0.65, .65);
        glutSolidSphere(0.5, 30, 30);
        glPopMatrix();
        glTranslatef(-2.0, 0, 0);
    }
    glPopMatrix();
    
    
    //  Update audio attack data for facial animation (eyebrows and mouth)
    _head.audioAttack = 0.9 * _head.audioAttack + 0.1 * fabs(_audioLoudness - _head.lastLoudness);
    _head.lastLoudness = _audioLoudness;
    
    
    const float BROW_LIFT_THRESHOLD = 100;
    if (_head.audioAttack > BROW_LIFT_THRESHOLD)
        _head.browAudioLift += sqrt(_head.audioAttack) / 1000.0;
    
    _head.browAudioLift *= .90;
    
    
    //  Render Eyebrows
    glPushMatrix();
    glTranslatef(-_head.interBrowDistance / 2.0,0.4,0.45);
    for(side = 0; side < 2; side++) {
        glColor3fv(browColor);
        glPushMatrix();
        glTranslatef(0, 0.35 + _head.browAudioLift, 0);
        glRotatef(_head.eyebrowPitch[side]/2.0, 1, 0, 0);
        glRotatef(_head.eyebrowRoll[side]/2.0, 0, 0, 1);
        glScalef(browWidth, browThickness, 1);
        glutSolidCube(0.5);
        glPopMatrix();
        glTranslatef(_head.interBrowDistance, 0, 0);
    }
    glPopMatrix();
    
    
    // Mouth
    
    glPushMatrix();
    glTranslatef(0,-0.35,0.75);
    glColor3f(0,0,0);
    glRotatef(_head.mouthPitch, 1, 0, 0);
    glRotatef(_head.mouthYaw, 0, 0, 1);
    glScalef(_head.mouthWidth*(.7 + sqrt(_head.averageLoudness)/60.0), _head.mouthHeight*(1.0 + sqrt(_head.averageLoudness)/30.0), 1);
    glutSolidCube(0.5);
    glPopMatrix();
    
    glTranslatef(0, 1.0, 0);
    
    glTranslatef(-_head.interPupilDistance/2.0,-0.68,0.7);
    // Right Eye
    glRotatef(-10, 1, 0, 0);
    glColor3fv(eyeColor);
    glPushMatrix();
    {
        glTranslatef(_head.interPupilDistance/10.0, 0, 0.05);
        glRotatef(20, 0, 0, 1);
        glScalef(_head.eyeballScaleX, _head.eyeballScaleY, _head.eyeballScaleZ);
        glutSolidSphere(0.25, 30, 30);
    }
    glPopMatrix();
    
    // Right Pupil
    if (_sphere == NULL) {
        _sphere = gluNewQuadric();
        gluQuadricTexture(_sphere, GL_TRUE);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        gluQuadricOrientation(_sphere, GLU_OUTSIDE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, iris_texture_width, iris_texture_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, &iris_texture[0]);
    }
    
    glPushMatrix();
    {
        glRotatef(_head.eyeballPitch[1], 1, 0, 0);
        glRotatef(_head.eyeballYaw[1] + _headYaw + _head.pupilConverge, 0, 1, 0);
        glTranslatef(0,0,.35);
        glRotatef(-75,1,0,0);
        glScalef(1.0, 0.4, 1.0);
        
        glEnable(GL_TEXTURE_2D);
        gluSphere(_sphere, _head.pupilSize, 15, 15);
        glDisable(GL_TEXTURE_2D);
    }
    
    glPopMatrix();
    // Left Eye
    glColor3fv(eyeColor);
    glTranslatef(_head.interPupilDistance, 0, 0);
    glPushMatrix();
    {
        glTranslatef(-_head.interPupilDistance/10.0, 0, .05);
        glRotatef(-20, 0, 0, 1);
        glScalef(_head.eyeballScaleX, _head.eyeballScaleY, _head.eyeballScaleZ);
        glutSolidSphere(0.25, 30, 30);
    }
    glPopMatrix();
    // Left Pupil
    glPushMatrix();
    {
        glRotatef(_head.eyeballPitch[0], 1, 0, 0);
        glRotatef(_head.eyeballYaw[0] + _headYaw - _head.pupilConverge, 0, 1, 0);
        glTranslatef(0, 0, .35);
        glRotatef(-75, 1, 0, 0);
        glScalef(1.0, 0.4, 1.0);
        
        glEnable(GL_TEXTURE_2D);
        gluSphere(_sphere, _head.pupilSize, 15, 15);
        glDisable(GL_TEXTURE_2D);
    }
    
    glPopMatrix();
    
    
    glPopMatrix();
 }

void Avatar::setHandMovementValues( glm::vec3 handOffset ) {
	_movedHandOffset = handOffset;
}

AvatarMode Avatar::getMode() {
	return _mode;
}

void Avatar::initializeSkeleton() {
    
	for (int b=0; b<NUM_AVATAR_BONES; b++) {
        _bone[b].isCollidable        = true;
        _bone[b].parent              = AVATAR_BONE_NULL;
        _bone[b].position			 = glm::vec3( 0.0, 0.0, 0.0 );
        _bone[b].defaultPosePosition = glm::vec3( 0.0, 0.0, 0.0 );
        _bone[b].springyPosition     = glm::vec3( 0.0, 0.0, 0.0 );
        _bone[b].springyVelocity     = glm::vec3( 0.0, 0.0, 0.0 );
        _bone[b].rotation            = glm::quat( 0.0f, 0.0f, 0.0f, 0.0f );
        _bone[b].yaw                 = 0.0;
        _bone[b].pitch               = 0.0;
        _bone[b].roll                = 0.0;
        _bone[b].length              = 0.0;
        _bone[b].radius              = 0.0;
        _bone[b].springBodyTightness = 4.0;
        _bone[b].orientation.setToIdentity();
	}
    
	// specify the parental hierarchy
	_bone[ AVATAR_BONE_PELVIS_SPINE		].parent = AVATAR_BONE_NULL;
	_bone[ AVATAR_BONE_MID_SPINE        ].parent = AVATAR_BONE_PELVIS_SPINE;
	_bone[ AVATAR_BONE_CHEST_SPINE		].parent = AVATAR_BONE_MID_SPINE;
	_bone[ AVATAR_BONE_NECK				].parent = AVATAR_BONE_CHEST_SPINE;
	_bone[ AVATAR_BONE_HEAD				].parent = AVATAR_BONE_NECK;
	_bone[ AVATAR_BONE_LEFT_CHEST		].parent = AVATAR_BONE_MID_SPINE;
	_bone[ AVATAR_BONE_LEFT_SHOULDER    ].parent = AVATAR_BONE_LEFT_CHEST;
	_bone[ AVATAR_BONE_LEFT_UPPER_ARM	].parent = AVATAR_BONE_LEFT_SHOULDER;
	_bone[ AVATAR_BONE_LEFT_FOREARM		].parent = AVATAR_BONE_LEFT_UPPER_ARM;
	_bone[ AVATAR_BONE_LEFT_HAND		].parent = AVATAR_BONE_LEFT_FOREARM;
	_bone[ AVATAR_BONE_RIGHT_CHEST		].parent = AVATAR_BONE_MID_SPINE;
	_bone[ AVATAR_BONE_RIGHT_SHOULDER	].parent = AVATAR_BONE_RIGHT_CHEST;
	_bone[ AVATAR_BONE_RIGHT_UPPER_ARM	].parent = AVATAR_BONE_RIGHT_SHOULDER;
	_bone[ AVATAR_BONE_RIGHT_FOREARM	].parent = AVATAR_BONE_RIGHT_UPPER_ARM;
	_bone[ AVATAR_BONE_RIGHT_HAND		].parent = AVATAR_BONE_RIGHT_FOREARM;
	_bone[ AVATAR_BONE_LEFT_PELVIS		].parent = AVATAR_BONE_PELVIS_SPINE;
	_bone[ AVATAR_BONE_LEFT_THIGH		].parent = AVATAR_BONE_LEFT_PELVIS;
	_bone[ AVATAR_BONE_LEFT_SHIN		].parent = AVATAR_BONE_LEFT_THIGH;
	_bone[ AVATAR_BONE_LEFT_FOOT		].parent = AVATAR_BONE_LEFT_SHIN;
	_bone[ AVATAR_BONE_RIGHT_PELVIS		].parent = AVATAR_BONE_PELVIS_SPINE;
	_bone[ AVATAR_BONE_RIGHT_THIGH		].parent = AVATAR_BONE_RIGHT_PELVIS;
	_bone[ AVATAR_BONE_RIGHT_SHIN		].parent = AVATAR_BONE_RIGHT_THIGH;
	_bone[ AVATAR_BONE_RIGHT_FOOT		].parent = AVATAR_BONE_RIGHT_SHIN;
    
	// specify the default pose position
	_bone[ AVATAR_BONE_PELVIS_SPINE		].defaultPosePosition = glm::vec3(  0.0,   0.0,   0.0  );
	_bone[ AVATAR_BONE_MID_SPINE		].defaultPosePosition = glm::vec3(  0.0,   0.1,   0.0  );
	_bone[ AVATAR_BONE_CHEST_SPINE		].defaultPosePosition = glm::vec3(  0.0,   0.06,  0.0  );
	_bone[ AVATAR_BONE_NECK				].defaultPosePosition = glm::vec3(  0.0,   0.06,  0.0  );
	_bone[ AVATAR_BONE_HEAD				].defaultPosePosition = glm::vec3(  0.0,   0.06,  0.0  );
	_bone[ AVATAR_BONE_LEFT_CHEST		].defaultPosePosition = glm::vec3( -0.05,  0.05,  0.0  );
	_bone[ AVATAR_BONE_LEFT_SHOULDER	].defaultPosePosition = glm::vec3( -0.03,  0.0,   0.0  );
	_bone[ AVATAR_BONE_LEFT_UPPER_ARM	].defaultPosePosition = glm::vec3(  0.0,  -0.1,   0.0  );
	_bone[ AVATAR_BONE_LEFT_FOREARM		].defaultPosePosition = glm::vec3(  0.0,  -0.1,   0.0  );
	_bone[ AVATAR_BONE_LEFT_HAND		].defaultPosePosition = glm::vec3(  0.0,  -0.05,  0.0  );
	_bone[ AVATAR_BONE_RIGHT_CHEST		].defaultPosePosition = glm::vec3(  0.05,  0.05,  0.0  );
	_bone[ AVATAR_BONE_RIGHT_SHOULDER	].defaultPosePosition = glm::vec3(  0.03,  0.0,   0.0  );
    _bone[ AVATAR_BONE_RIGHT_UPPER_ARM	].defaultPosePosition = glm::vec3(  0.0,  -0.1,   0.0  );
    _bone[ AVATAR_BONE_RIGHT_FOREARM	].defaultPosePosition = glm::vec3(  0.0,  -0.1,   0.0  );
	_bone[ AVATAR_BONE_RIGHT_HAND		].defaultPosePosition = glm::vec3(  0.0,  -0.05,  0.0  );
	_bone[ AVATAR_BONE_LEFT_PELVIS		].defaultPosePosition = glm::vec3( -0.05,  0.0,   0.0  );
	_bone[ AVATAR_BONE_LEFT_THIGH		].defaultPosePosition = glm::vec3(  0.0,  -0.15,  0.0  );
	_bone[ AVATAR_BONE_LEFT_SHIN		].defaultPosePosition = glm::vec3(  0.0,  -0.15,  0.0  );
	_bone[ AVATAR_BONE_LEFT_FOOT		].defaultPosePosition = glm::vec3(  0.0,   0.0,   0.04 );
	_bone[ AVATAR_BONE_RIGHT_PELVIS		].defaultPosePosition = glm::vec3(  0.05,  0.0,   0.0  );
	_bone[ AVATAR_BONE_RIGHT_THIGH		].defaultPosePosition = glm::vec3(  0.0,  -0.15,  0.0  );
	_bone[ AVATAR_BONE_RIGHT_SHIN		].defaultPosePosition = glm::vec3(  0.0,  -0.15,  0.0  );
	_bone[ AVATAR_BONE_RIGHT_FOOT		].defaultPosePosition = glm::vec3(  0.0,   0.0,   0.04 );
    
	// specify the radii of the bone positions
	_bone[ AVATAR_BONE_PELVIS_SPINE		].radius = 0.05;
	_bone[ AVATAR_BONE_MID_SPINE		].radius = 0.06;
	_bone[ AVATAR_BONE_CHEST_SPINE		].radius = 0.03;
	_bone[ AVATAR_BONE_NECK				].radius = 0.02;
	_bone[ AVATAR_BONE_HEAD				].radius = 0.02;
	_bone[ AVATAR_BONE_LEFT_CHEST		].radius = 0.025;
	_bone[ AVATAR_BONE_LEFT_SHOULDER	].radius = 0.02;
	_bone[ AVATAR_BONE_LEFT_UPPER_ARM	].radius = 0.015;
	_bone[ AVATAR_BONE_LEFT_FOREARM		].radius = 0.015;
	_bone[ AVATAR_BONE_LEFT_HAND		].radius = 0.01;
	_bone[ AVATAR_BONE_RIGHT_CHEST		].radius = 0.025;
	_bone[ AVATAR_BONE_RIGHT_SHOULDER	].radius = 0.02;
	_bone[ AVATAR_BONE_RIGHT_UPPER_ARM	].radius = 0.015;
	_bone[ AVATAR_BONE_RIGHT_FOREARM	].radius = 0.015;
	_bone[ AVATAR_BONE_RIGHT_HAND		].radius = 0.01;
	_bone[ AVATAR_BONE_LEFT_PELVIS		].radius = 0.02;
	_bone[ AVATAR_BONE_LEFT_THIGH		].radius = 0.02;
	_bone[ AVATAR_BONE_LEFT_SHIN		].radius = 0.015;
	_bone[ AVATAR_BONE_LEFT_FOOT		].radius = 0.02;
	_bone[ AVATAR_BONE_RIGHT_PELVIS		].radius = 0.02;
	_bone[ AVATAR_BONE_RIGHT_THIGH		].radius = 0.02;
	_bone[ AVATAR_BONE_RIGHT_SHIN		].radius = 0.015;
	_bone[ AVATAR_BONE_RIGHT_FOOT		].radius = 0.02;

	// to aid in hand-shaking and hand-holding, the right hand is not collidable
	_bone[ AVATAR_BONE_RIGHT_UPPER_ARM	].isCollidable = false;
	_bone[ AVATAR_BONE_RIGHT_FOREARM	].isCollidable = false;
	_bone[ AVATAR_BONE_RIGHT_HAND		].isCollidable = false; 
       
	// calculate bone length
	calculateBoneLengths();
    
    _pelvisStandingHeight = 
	_bone[ AVATAR_BONE_LEFT_FOOT    ].radius +
	_bone[ AVATAR_BONE_LEFT_SHIN    ].length +
	_bone[ AVATAR_BONE_LEFT_THIGH   ].length +
    _bone[ AVATAR_BONE_PELVIS_SPINE ].length;
    //printf( "_pelvisStandingHeight = %f\n", _pelvisStandingHeight );
    
    _height = 
    (
        _pelvisStandingHeight +
        _bone[ AVATAR_BONE_MID_SPINE  ].length +
        _bone[ AVATAR_BONE_CHEST_SPINE].length +
        _bone[ AVATAR_BONE_NECK		  ].length +
        _bone[ AVATAR_BONE_HEAD		  ].length +
        _bone[ AVATAR_BONE_HEAD		  ].radius
    );
    
	// generate world positions
	updateSkeleton();
}

void Avatar::calculateBoneLengths() {
	for (int b = 0; b < NUM_AVATAR_BONES; b++) {
		_bone[b].length = glm::length( _bone[b].defaultPosePosition );
	}
    
	_maxArmLength
	= _bone[ AVATAR_BONE_RIGHT_UPPER_ARM ].length
	+ _bone[ AVATAR_BONE_RIGHT_FOREARM	 ].length
	+ _bone[ AVATAR_BONE_RIGHT_HAND		 ].length;
}

void Avatar::updateSkeleton() {
	// rotate body...
	_orientation.setToIdentity();
	_orientation.yaw( _bodyYaw );
    
	// calculate positions of all bones by traversing the skeleton tree:
	for (int b = 0; b < NUM_AVATAR_BONES; b++) {
		if ( _bone[b].parent == AVATAR_BONE_NULL ) {
            _bone[b].orientation.set( _orientation );
			_bone[b].position = _position;
		}
		else {
			_bone[b].orientation.set( _bone[ _bone[b].parent ].orientation );
			_bone[b].position = _bone[ _bone[b].parent ].position;
		}
        
        // if this is not my avatar, then hand position comes from transmitted data
        if ( ! _isMine ) {
            _bone[ AVATAR_BONE_RIGHT_HAND ].position = _handPosition;
        }
        
        // the following will be replaced by a proper rotation...
		float xx = glm::dot( _bone[b].defaultPosePosition, _bone[b].orientation.getRight() );
		float yy = glm::dot( _bone[b].defaultPosePosition, _bone[b].orientation.getUp	() );
		float zz = glm::dot( _bone[b].defaultPosePosition, _bone[b].orientation.getFront() );
        
		glm::vec3 rotatedBoneVector( xx, yy, zz );
        
        //glm::vec3 myEuler ( 0.0f, 0.0f, 0.0f );
        //glm::quat myQuat ( myEuler );
        
		_bone[b].position += rotatedBoneVector;
	}
}

void Avatar::initializeBodySprings() {
	for (int b = 0; b < NUM_AVATAR_BONES; b++) {
		_bone[b].springyPosition = _bone[b].position;
		_bone[b].springyVelocity = glm::vec3( 0.0f, 0.0f, 0.0f );
	}
}


void Avatar::updateBodySprings( float deltaTime ) {
	for (int b = 0; b < NUM_AVATAR_BONES; b++) {
		glm::vec3 springVector( _bone[b].springyPosition );
        
		if ( _bone[b].parent == AVATAR_BONE_NULL ) {
			springVector -= _position;
		}
		else {
			springVector -= _bone[ _bone[b].parent ].springyPosition;
		}
        
		float length = glm::length( springVector );
		
		if ( length > 0.0f ) {
			glm::vec3 springDirection = springVector / length;
			
			float force = (length - _bone[b].length) * BODY_SPRING_FORCE * deltaTime;
			
			_bone[b].springyVelocity -= springDirection * force;
            
            if ( _bone[b].parent != AVATAR_BONE_NULL ) {
                _bone[_bone[b].parent].springyVelocity += springDirection * force;
            }
		}
        
		_bone[b].springyVelocity += (_bone[b].position - _bone[b].springyPosition) * _bone[b].springBodyTightness * deltaTime;
        
		float decay = 1.0 - BODY_SPRING_DECAY * deltaTime;
		
		if (decay > 0.0) {
			_bone[b].springyVelocity *= decay;
		}
		else {
			_bone[b].springyVelocity = glm::vec3( 0.0f, 0.0f, 0.0f );
		}
        
		_bone[b].springyPosition += _bone[b].springyVelocity;
	}
}

const glm::vec3& Avatar::getHeadPosition() const {
    
    if (_usingBodySprings) {
        return _bone[ AVATAR_BONE_HEAD ].springyPosition;
    }
    
    return _bone[ AVATAR_BONE_HEAD ].position;
}

void Avatar::updateHandMovement( float deltaTime ) {
	glm::vec3 transformedHandMovement;
    
	transformedHandMovement
	= _orientation.getRight() *  _movedHandOffset.x
	+ _orientation.getUp()	  * -_movedHandOffset.y * 0.5f
	+ _orientation.getFront() * -_movedHandOffset.y;
    
	_bone[ AVATAR_BONE_RIGHT_HAND ].position += transformedHandMovement;
    
    if (_isMine) {
        _handState = _mousePressed;
    }
}


void Avatar::updateArmIKAndConstraints( float deltaTime ) {
    
	// determine the arm vector
	glm::vec3 armVector = _bone[ AVATAR_BONE_RIGHT_HAND ].position;
	armVector -= _bone[ AVATAR_BONE_RIGHT_SHOULDER ].position;
    
	// test to see if right hand is being dragged beyond maximum arm length
	float distance = glm::length( armVector );
	
	// don't let right hand get dragged beyond maximum arm length...
	if ( distance > _maxArmLength ) {
		// reset right hand to be constrained to maximum arm length
		_bone[ AVATAR_BONE_RIGHT_HAND ].position = _bone[ AVATAR_BONE_RIGHT_SHOULDER ].position;
		glm::vec3 armNormal = armVector / distance;
		armVector = armNormal * _maxArmLength;
		distance = _maxArmLength;
		glm::vec3 constrainedPosition = _bone[ AVATAR_BONE_RIGHT_SHOULDER ].position;
		constrainedPosition += armVector;
		_bone[ AVATAR_BONE_RIGHT_HAND ].position = constrainedPosition;
	}
    
	// set elbow position
	glm::vec3 newElbowPosition = _bone[ AVATAR_BONE_RIGHT_SHOULDER ].position;
	newElbowPosition += armVector * ONE_HALF;
	glm::vec3 perpendicular = glm::cross( _orientation.getFront(), armVector );
	newElbowPosition += perpendicular * ( 1.0f - ( _maxArmLength / distance ) ) * ONE_HALF;
	_bone[ AVATAR_BONE_RIGHT_UPPER_ARM ].position = newElbowPosition;
    
	// set wrist position
	glm::vec3 vv( _bone[ AVATAR_BONE_RIGHT_HAND ].position );
	vv -= _bone[ AVATAR_BONE_RIGHT_UPPER_ARM ].position;
	glm::vec3 newWristPosition = _bone[ AVATAR_BONE_RIGHT_UPPER_ARM ].position;
	newWristPosition += vv * 0.7f;
	_bone[ AVATAR_BONE_RIGHT_FOREARM ].position = newWristPosition;
}




void Avatar::renderBody() {
    
    //  Render bone positions as spheres
	for (int b = 0; b < NUM_AVATAR_BONES; b++) {
        
        if ( b != AVATAR_BONE_HEAD ) { // the head is rendered as a special case in "renderHead"
    
            //render bone orientation
            //renderOrientationDirections( _bone[b].springyPosition, _bone[b].orientation, _bone[b].radius * 2.0 );
            
            if ( _usingBodySprings ) {
                glColor3fv( skinColor );
                glPushMatrix();
                glTranslatef( _bone[b].springyPosition.x, _bone[b].springyPosition.y, _bone[b].springyPosition.z );
                glutSolidSphere( _bone[b].radius, 20.0f, 20.0f );
                glPopMatrix();
            }
            else {
                glColor3fv( skinColor );
                glPushMatrix();
                glTranslatef( _bone[b].position.x, _bone[b].position.y, _bone[b].position.z );
                glutSolidSphere( _bone[b].radius, 20.0f, 20.0f );
                glPopMatrix();
            }
        }
	}
    
    // Render lines connecting the bone positions
	if ( _usingBodySprings ) {
		glColor3f( 0.4f, 0.5f, 0.6f );
		glLineWidth(3.0);
        
		for (int b = 1; b < NUM_AVATAR_BONES; b++) {
            if ( _bone[b].parent != AVATAR_BONE_NULL ) {
                glBegin( GL_LINE_STRIP );
                glVertex3fv( &_bone[ _bone[ b ].parent ].springyPosition.x );
                glVertex3fv( &_bone[ b ].springyPosition.x );
                glEnd();
            }
		}
	}
	else {
		glColor3fv( skinColor );
		glLineWidth(3.0);
        
		for (int b = 1; b < NUM_AVATAR_BONES; b++) {
            if ( _bone[b].parent != AVATAR_BONE_NULL ) {
                glBegin( GL_LINE_STRIP );
                glVertex3fv( &_bone[ _bone[ b ].parent ].position.x );
                glVertex3fv( &_bone[ b ].position.x);
                glEnd();
            }
		}
	}
	
    // if the hand is grasping, show it...
	if (( _usingBodySprings ) && ( _handState == 1 )) {
		glPushMatrix();
        glTranslatef(_bone[ AVATAR_BONE_RIGHT_HAND ].springyPosition.x,
                     _bone[ AVATAR_BONE_RIGHT_HAND ].springyPosition.y,
                     _bone[ AVATAR_BONE_RIGHT_HAND ].springyPosition.z);
        glColor4f( 1.0, 1.0, 0.8, 0.3 ); glutSolidSphere( 0.020f, 10.0f, 10.0f );
        glColor4f( 1.0, 1.0, 0.4, 0.2 ); glutSolidSphere( 0.025f, 10.0f, 10.0f );
        glColor4f( 1.0, 1.0, 0.2, 0.1 ); glutSolidSphere( 0.030f, 10.0f, 10.0f );
        
		glPopMatrix();
	}
}

void Avatar::SetNewHeadTarget(float pitch, float yaw) {
    _head.pitchTarget = pitch;
    _head.yawTarget   = yaw;
}

// Process UDP interface data from Android transmitter or Google Glass
void Avatar::processTransmitterData(unsigned char* packetData, int numBytes) {
    //  Read a packet from a transmitter app, process the data
    float
    accX, accY, accZ,           //  Measured acceleration
    graX, graY, graZ,           //  Gravity
    gyrX, gyrY, gyrZ,           //  Gyro velocity in radians/sec as (pitch, roll, yaw)
    linX, linY, linZ,           //  Linear Acceleration (less gravity)
    rot1, rot2, rot3, rot4;     //  Rotation of device:
                                //    rot1 = roll, ranges from -1 to 1, 0 = flat on table
                                //    rot2 = pitch, ranges from -1 to 1, 0 = flat on table
                                //    rot3 = yaw, ranges from -1 to 1 
    
    const bool IS_GLASS = false;         //  Whether to assume this is a Google glass transmitting

    sscanf((char *)packetData, "tacc %f %f %f gra %f %f %f gyr %f %f %f lin %f %f %f rot %f %f %f %f",
           &accX, &accY, &accZ,
           &graX, &graY, &graZ,
           &gyrX, &gyrY, &gyrZ,
           &linX, &linY, &linZ,
           &rot1, &rot2, &rot3, &rot4);
    
    if (_transmitterPackets++ == 0) {
        // If first packet received, note time, turn head spring return OFF, get start rotation
        gettimeofday(&_transmitterTimer, NULL);
        if (IS_GLASS) {
            setHeadReturnToCenter(true);
            setHeadSpringScale(10.f);
            printLog("Using Google Glass to drive head, springs ON.\n");

        } else {
            setHeadReturnToCenter(false);
            printLog("Using Transmitter to drive head, springs OFF.\n");

        }
        _transmitterInitialReading = glm::vec3(     rot3,
                                                    rot2,
                                                    rot1 );
    }
    const int TRANSMITTER_COUNT = 100;
    if (_transmitterPackets % TRANSMITTER_COUNT == 0) {
        // Every 100 packets, record the observed Hz of the transmitter data
        timeval now;
        gettimeofday(&now, NULL);
        double msecsElapsed = diffclock(&_transmitterTimer, &now);
        _transmitterHz = static_cast<float>( (double)TRANSMITTER_COUNT / (msecsElapsed / 1000.0) );
        _transmitterTimer = now;
        printLog("Transmitter Hz: %3.1f\n", _transmitterHz);
    }
    //printLog("Gyr: %3.1f, %3.1f, %3.1f\n", glm::degrees(gyrZ), glm::degrees(-gyrX), glm::degrees(gyrY));
    //printLog("Rot: %3.1f, %3.1f, %3.1f, %3.1f\n", rot1, rot2, rot3, rot4);
    
    //  Update the head with the transmitter data
    glm::vec3 eulerAngles((rot3 - _transmitterInitialReading.x) * 180.f,
                          -(rot2 - _transmitterInitialReading.y) * 180.f,
                          (rot1 - _transmitterInitialReading.z) * 180.f);
    if (eulerAngles.x > 180.f) { eulerAngles.x -= 360.f; }
    if (eulerAngles.x < -180.f) { eulerAngles.x += 360.f; }
    
    glm::vec3 angularVelocity;
    if (!IS_GLASS) {
        angularVelocity = glm::vec3(glm::degrees(gyrZ), glm::degrees(-gyrX), glm::degrees(gyrY));
        setHeadFromGyros( &eulerAngles, &angularVelocity,
                         (_transmitterHz == 0.f) ? 0.f : 1.f / _transmitterHz, 1.0);

    } else {
        angularVelocity = glm::vec3(glm::degrees(gyrY), glm::degrees(-gyrX), glm::degrees(-gyrZ));
        setHeadFromGyros( &eulerAngles, &angularVelocity,
                         (_transmitterHz == 0.f) ? 0.f : 1.f / _transmitterHz, 1000.0);

    }
    
}

void Avatar::setHeadFromGyros(glm::vec3* eulerAngles, glm::vec3* angularVelocity, float deltaTime, float smoothingTime) {
    //
    //  Given absolute position and angular velocity information, update the avatar's head angles
    //  with the goal of fast instantaneous updates that gradually follow the absolute data.
    //
    //  Euler Angle format is (Yaw, Pitch, Roll) in degrees
    //
    //  Angular Velocity is (Yaw, Pitch, Roll) in degrees per second
    //
    //  SMOOTHING_TIME is the time is seconds over which the head should average to the
    //  absolute eulerAngles passed.
    //  
    //
    float const MAX_YAW = 90.f;
    float const MIN_YAW = -90.f;
    float const MAX_PITCH = 85.f;
    float const MIN_PITCH = -85.f;
    float const MAX_ROLL = 90.f;
    float const MIN_ROLL = -90.f;
    
    if (deltaTime == 0.f) {
        //  On first sample, set head to absolute position
        setHeadYaw(eulerAngles->x);
        setHeadPitch(eulerAngles->y);
        setHeadRoll(eulerAngles->z);
    } else { 
        glm::vec3 angles(getHeadYaw(), getHeadPitch(), getHeadRoll());
        //  Increment by detected velocity 
        angles += (*angularVelocity) * deltaTime;
        //  Smooth to slowly follow absolute values
        angles = ((1.f - deltaTime / smoothingTime) * angles) + (deltaTime / smoothingTime) * (*eulerAngles);
        setHeadYaw(fmin(fmax(angles.x, MIN_YAW), MAX_YAW));
        setHeadPitch(fmin(fmax(angles.y, MIN_PITCH), MAX_PITCH));
        setHeadRoll(fmin(fmax(angles.z, MIN_ROLL), MAX_ROLL));
        //printLog("Y/P/R: %3.1f, %3.1f, %3.1f\n", angles.x, angles.y, angles.z);
    }
}

//  Find and return the gravity vector at my location
glm::vec3 Avatar::getGravity(glm::vec3 pos) {
    //
    //  For now, we'll test this with a simple global lookup, but soon we will add getting this
    //  from the domain/voxelserver (or something similar)
    //
    if (glm::length(pos) < 5.f)  {
        //  If near the origin sphere, turn gravity ON
        return glm::vec3(0.f, -1.f, 0.f);
    } else {
        //  If flying in space, turn gravity OFF
        return glm::vec3(0.f, 0.f, 0.f);
    }
}