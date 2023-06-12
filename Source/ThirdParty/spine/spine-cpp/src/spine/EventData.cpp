/******************************************************************************
 * Spine Runtimes License Agreement
 * Last updated September 24, 2021. Replaces all prior versions.
 *
 * Copyright (c) 2013-2021, Esoteric Software LLC
 *
 * Integration of the Spine Runtimes into software or otherwise creating
 * derivative works of the Spine Runtimes is permitted under the terms and
 * conditions of Section 2 of the Spine Editor License Agreement:
 * http://esotericsoftware.com/spine-editor-license
 *
 * Otherwise, it is permitted to integrate the Spine Runtimes into software
 * or otherwise create derivative works of the Spine Runtimes (collectively,
 * "Products"), provided that each user of the Products must obtain their own
 * Spine Editor license and redistribution of the Products in any form must
 * include this license and copyright notice.
 *
 * THE SPINE RUNTIMES ARE PROVIDED BY ESOTERIC SOFTWARE LLC "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL ESOTERIC SOFTWARE LLC BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES,
 * BUSINESS INTERRUPTION, OR LOSS OF USE, DATA, OR PROFITS) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THE SPINE RUNTIMES, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *****************************************************************************/

#include <spine/SpineEventData.h>

#include <assert.h>

namespace spine
{

spine::SpineEventData::SpineEventData(const spine::String &name) : _name(name),
														 _intValue(0),
														 _floatValue(0),
														 _stringValue(),
														 _audioPath(),
														 _volume(1),
														 _balance(0) {
	assert(_name.length() > 0);
}

/// The name of the SpineEvent, which is unique within the skeleton.
const spine::String &spine::SpineEventData::getName() const {
	return _name;
}

int spine::SpineEventData::getIntValue() const {
	return _intValue;
}

void spine::SpineEventData::setIntValue(int inValue) {
	_intValue = inValue;
}

float spine::SpineEventData::getFloatValue() const {
	return _floatValue;
}

void spine::SpineEventData::setFloatValue(float inValue) {
	_floatValue = inValue;
}

const spine::String &spine::SpineEventData::getStringValue() const {
	return _stringValue;
}

void spine::SpineEventData::setStringValue(const spine::String &inValue) {
	this->_stringValue = inValue;
}

const spine::String &spine::SpineEventData::getAudioPath() const {
	return _audioPath;
}

void spine::SpineEventData::setAudioPath(const spine::String &inValue) {
	_audioPath = inValue;
}


float spine::SpineEventData::getVolume() const {
	return _volume;
}

void spine::SpineEventData::setVolume(float inValue) {
	_volume = inValue;
}

float spine::SpineEventData::getBalance() const {
	return _balance;
}

void spine::SpineEventData::setBalance(float inValue) {
	_balance = inValue;
}

}
