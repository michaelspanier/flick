/*
 * Copyright 2026, Michael Spanier.
 * Distributed under the terms of the MIT License.
 *
 * Based on the VideoConsumer of CodyCam.
 *
 * SPDX-License-Identifier: MIT
 */


#include "FlickConsumer.h"

#include <Autolock.h>
#include <Buffer.h>
#include <MediaDefs.h>
#include <OS.h>
#include <TimeSource.h>

#include <stdio.h>
#include <string.h>


FlickConsumer::FlickConsumer(const BMessenger& target)
	:
	BMediaNode("Flick"),
	BMediaEventLooper(),
	BBufferConsumer(B_MEDIA_RAW_VIDEO),
	fTarget(target),
	fLock("flick frame"),
	fFrame(NULL),
	fWidth(0),
	fHeight(0),
	fHasFrame(false),
	fConnected(false),
	fFramePending(0)
{
	AddNodeKind(B_PHYSICAL_OUTPUT);
	SetEventLatency(0);
	SetPriority(B_DISPLAY_PRIORITY);
}


FlickConsumer::~FlickConsumer()
{
	Quit();
	delete fFrame;
}


bool
FlickConsumer::CopyFrame(BBitmap* bitmap)
{
	BAutolock locker(fLock);
	if (fFrame == NULL || !fHasFrame || bitmap == NULL
		|| bitmap->BitsLength() != fFrame->BitsLength()) {
		return false;
	}

	memcpy(bitmap->Bits(), fFrame->Bits(), fFrame->BitsLength());
	atomic_set(&fFramePending, 0);
	return true;
}


BMediaAddOn*
FlickConsumer::AddOn(int32* cookie) const
{
	*cookie = 0;
	return NULL;
}


void
FlickConsumer::NodeRegistered()
{
	fInput.destination.port = ControlPort();
	fInput.destination.id = 0;
	fInput.source = media_source::null;
	fInput.format.type = B_MEDIA_RAW_VIDEO;
	fInput.format.u.raw_video = media_raw_video_format::wildcard;
	fInput.node = Node();
	strlcpy(fInput.name, "Flick video", sizeof(fInput.name));

	Run();
}


void
FlickConsumer::HandleEvent(const media_timed_event* event, bigtime_t lateness,
	bool realTimeEvent)
{
	// Buffers are shown as soon as they arrive, see BufferReceived().
}


status_t
FlickConsumer::AcceptFormat(const media_destination& dest,
	media_format* format)
{
	if (dest != fInput.destination)
		return B_MEDIA_BAD_DESTINATION;

	if (format->type == B_MEDIA_NO_TYPE)
		format->type = B_MEDIA_RAW_VIDEO;
	if (format->type != B_MEDIA_RAW_VIDEO)
		return B_MEDIA_BAD_FORMAT;

	color_space space = format->u.raw_video.display.format;
	if (space == media_raw_video_format::wildcard.display.format)
		format->u.raw_video.display.format = B_RGB32;
	else if (space != B_RGB32)
		return B_MEDIA_BAD_FORMAT;

	return B_OK;
}


status_t
FlickConsumer::GetNextInput(int32* cookie, media_input* input)
{
	if (*cookie != 0)
		return B_BAD_INDEX;

	*input = fInput;
	*cookie = 1;
	return B_OK;
}


void
FlickConsumer::DisposeInputCookie(int32 cookie)
{
}


void
FlickConsumer::BufferReceived(BBuffer* buffer)
{
	bool use = false;
	{
		BAutolock locker(fLock);
		if (fConnected && fFrame != NULL
			&& RunState() == B_STARTED
			&& buffer->SizeUsed() >= (size_t)fFrame->BitsLength()) {
			memcpy(fFrame->Bits(), buffer->Data(), fFrame->BitsLength());
			fHasFrame = true;
			use = true;
		}
	}
	buffer->Recycle();

	// One message per frame that was not picked up, the view must not be
	// flooded.
	if (use && atomic_test_and_set(&fFramePending, 1, 0) == 0)
		fTarget.SendMessage(kMsgFlickFrame);
}


void
FlickConsumer::ProducerDataStatus(const media_destination& forWhom,
	int32 status, bigtime_t atMediaTime)
{
}


status_t
FlickConsumer::GetLatencyFor(const media_destination& forWhom,
	bigtime_t* latency, media_node_id* timeSource)
{
	if (forWhom != fInput.destination)
		return B_MEDIA_BAD_DESTINATION;

	*latency = 20000;
	*timeSource = TimeSource()->ID();
	return B_OK;
}


status_t
FlickConsumer::Connected(const media_source& producer,
	const media_destination& where, const media_format& format,
	media_input* input)
{
	fInput.source = producer;
	fInput.format = format;
	fInput.node = Node();
	*input = fInput;

	_SetFormat(format);
	fConnected = true;
	return B_OK;
}


void
FlickConsumer::Disconnected(const media_source& producer,
	const media_destination& where)
{
	if (where != fInput.destination || producer != fInput.source)
		return;

	BAutolock locker(fLock);
	fConnected = false;
	fHasFrame = false;
	fInput.source = media_source::null;
}


status_t
FlickConsumer::FormatChanged(const media_source& producer,
	const media_destination& consumer, int32 fromChangeCount,
	const media_format& format)
{
	if (consumer != fInput.destination)
		return B_MEDIA_BAD_DESTINATION;
	if (producer != fInput.source)
		return B_MEDIA_BAD_SOURCE;

	fInput.format = format;
	_SetFormat(format);
	return B_OK;
}


void
FlickConsumer::_SetFormat(const media_format& format)
{
	BAutolock locker(fLock);

	fWidth = format.u.raw_video.display.line_width;
	fHeight = format.u.raw_video.display.line_count;
	fHasFrame = false;

	delete fFrame;
	fFrame = new BBitmap(BRect(0, 0, fWidth - 1, fHeight - 1), B_RGB32);
	if (fFrame->InitCheck() != B_OK) {
		delete fFrame;
		fFrame = NULL;
	}
}
