/*
 * Copyright 2026, Michael Spanier.
 * Distributed under the terms of the MIT License.
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef FLICK_CONSUMER_H
#define FLICK_CONSUMER_H


#include <Bitmap.h>
#include <BufferConsumer.h>
#include <Locker.h>
#include <MediaEventLooper.h>
#include <Messenger.h>


/*!	Receives the video of a camera and keeps the newest frame. Each time a
	frame arrives the target gets the message \c kMsgFlickFrame, at most one
	per frame that was not picked up yet.
*/
class FlickConsumer : public BMediaEventLooper, public BBufferConsumer {
public:
								FlickConsumer(const BMessenger& target);
	virtual						~FlickConsumer();

	//! Copies the newest frame into \a bitmap, which must have the size of the
	//! connection. Returns false if there is no frame.
			bool				CopyFrame(BBitmap* bitmap);
			uint32				Width() const { return fWidth; }
			uint32				Height() const { return fHeight; }

	// BMediaNode
	virtual	BMediaAddOn*		AddOn(int32* cookie) const;

protected:
	virtual	void				NodeRegistered();

	// BMediaEventLooper
	virtual	void				HandleEvent(const media_timed_event* event,
									bigtime_t lateness, bool realTimeEvent);

	// BBufferConsumer
public:
	virtual	status_t			AcceptFormat(const media_destination& dest,
									media_format* format);
	virtual	status_t			GetNextInput(int32* cookie,
									media_input* input);
	virtual	void				DisposeInputCookie(int32 cookie);
	virtual	void				BufferReceived(BBuffer* buffer);
	virtual	void				ProducerDataStatus(
									const media_destination& forWhom,
									int32 status, bigtime_t atMediaTime);
	virtual	status_t			GetLatencyFor(
									const media_destination& forWhom,
									bigtime_t* latency,
									media_node_id* timeSource);
	virtual	status_t			Connected(const media_source& producer,
									const media_destination& where,
									const media_format& format,
									media_input* input);
	virtual	void				Disconnected(const media_source& producer,
									const media_destination& where);
	virtual	status_t			FormatChanged(const media_source& producer,
									const media_destination& consumer,
									int32 fromChangeCount,
									const media_format& format);

private:
			void				_SetFormat(const media_format& format);

			BMessenger			fTarget;
			media_input			fInput;
			BLocker				fLock;
			BBitmap*			fFrame;
			uint32				fWidth;
			uint32				fHeight;
			bool				fHasFrame;
			bool				fConnected;
			int32				fFramePending;
};


const uint32 kMsgFlickFrame = 'FlFr';


#endif	// FLICK_CONSUMER_H
