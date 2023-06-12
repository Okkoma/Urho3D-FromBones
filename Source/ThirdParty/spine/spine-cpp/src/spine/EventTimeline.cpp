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

#include <spine/SpineEventTimeline.h>

#include <spine/SpineEvent.h>
#include <spine/Skeleton.h>

#include <spine/Animation.h>
#include <spine/ContainerUtil.h>
#include <spine/SpineEventData.h>
#include <spine/Property.h>
#include <spine/Slot.h>
#include <spine/SlotData.h>

#include <float.h>


namespace spine
{

RTTI_IMPL(SpineEventTimeline, Timeline)

SpineEventTimeline::SpineEventTimeline(size_t frameCount) : Timeline(frameCount, 1) {
	PropertyId ids[] = {((PropertyId) Property_SpineEvent << 32)};
	setPropertyIds(ids, 1);
	_SpineEvents.setSize(frameCount, NULL);
}

SpineEventTimeline::~SpineEventTimeline() {
	ContainerUtil::cleanUpVectorOfPointers(_SpineEvents);
}

void SpineEventTimeline::apply(Skeleton &skeleton, float lastTime, float time, Vector<SpineEvent *> *pSpineEvents, float alpha,
						  MixBlend blend, MixDirection direction) {
	if (pSpineEvents == NULL) return;

	Vector<SpineEvent *> &SpineEvents = *pSpineEvents;

	size_t frameCount = _frames.size();

	if (lastTime > time) {
		// Fire SpineEvents after last time for looped animations.
		apply(skeleton, lastTime, FLT_MAX, pSpineEvents, alpha, blend, direction);
		lastTime = -1.0f;
	} else if (lastTime >= _frames[frameCount - 1]) {
		// Last time is after last i.
		return;
	}

	if (time < _frames[0]) return;// Time is before first i.

	int i;
	if (lastTime < _frames[0]) {
		i = 0;
	} else {
		i = Animation::search(_frames, lastTime) + 1;
		float frameTime = _frames[i];
		while (i > 0) {
			// Fire multiple SpineEvents with the same i.
			if (_frames[i - 1] != frameTime) break;
			i--;
		}
	}

	for (; (size_t) i < frameCount && time >= _frames[i]; i++)
		SpineEvents.add(_SpineEvents[i]);
}

void SpineEventTimeline::setFrame(size_t frame, SpineEvent *SpineEvent) {
	_frames[frame] = SpineEvent->getTime();
	_SpineEvents[frame] = SpineEvent;
}

Vector<SpineEvent *> &SpineEventTimeline::getSpineEvents() { return _SpineEvents; }

}
