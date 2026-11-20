/*
 * Copyright 2026, Michael Spanier.
 * Distributed under the terms of the MIT License.
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef FLICK_CAMERA_H
#define FLICK_CAMERA_H


#include <MediaAddOn.h>
#include <MediaDefs.h>
#include <MediaNode.h>
#include <MediaRoster.h>
#include <Messenger.h>

#include <vector>

#include "FlickLogic.h"

class BDiscreteParameter;
class BParameterWeb;
class FlickConsumer;


struct flick_camera_info {
			dormant_node_info	node;
			char				name[B_MEDIA_NAME_LENGTH];
};


/*!	One camera of the system, connected to our consumer. All calls come from
	the window thread.
*/
class FlickCamera {
public:
								FlickCamera(const BMessenger& target);
								~FlickCamera();

	//! Lists the cameras (video sources) the media server knows.
	static	void				List(std::vector<flick_camera_info>& cameras);

			status_t			Open(const flick_camera_info& camera);
			void				Close();

	//! Sizes the camera takes, found by trying the usual ones.
			int					FindSizes(flick_size* sizes, int maxCount);

			status_t			Start(uint32 width, uint32 height);
			void				Stop();
			bool				IsRunning() const { return fRunning; }

			FlickConsumer*		Consumer() const { return fConsumer; }

	//! The camera node, for programs that show its parameters.
			bool				HasProducer() const { return fHasProducer; }
			media_node			Producer() const { return fProducer; }

private:
			status_t			_FindLiveNode(const flick_camera_info& camera);
			status_t			_Connect(uint32 width, uint32 height);
			int					_SizesFromParameters(flick_size* sizes, int maxCount);
			int					_SizesByTrying(flick_size* sizes, int maxCount);
	static	BDiscreteParameter*	_FindResolution(BParameterWeb* web);
			void				_Disconnect();

			BMediaRoster*		fRoster;
			FlickConsumer*		fConsumer;
			media_node			fProducer;
			bool				fHasProducer;
			media_output		fOutput;
			media_input			fInput;
			bool				fConnected;
			bool				fRunning;
};


#endif	// FLICK_CAMERA_H
