/*
 * Copyright 2026, Michael Spanier.
 * Distributed under the terms of the MIT License.
 *
 * Based on the node set up of CodyCam.
 *
 * SPDX-License-Identifier: MIT
 */


#include "FlickCamera.h"

#include <MediaRoster.h>
#include <ParameterWeb.h>
#include <TimeSource.h>
#include <scheduler.h>

#include <stdio.h>
#include <string.h>

#include "FlickConsumer.h"


static const flick_size kCandidateSizes[] = {
	{ 160, 120 }, { 320, 180 }, { 320, 240 }, { 352, 288 }, { 424, 240 },
	{ 640, 360 }, { 640, 480 }, { 848, 480 }, { 960, 540 }, { 1280, 720 },
};

static const int kCandidateCount
	= sizeof(kCandidateSizes) / sizeof(kCandidateSizes[0]);


FlickCamera::FlickCamera(const BMessenger& target)
	:
	fRoster(NULL),
	fConsumer(NULL),
	fHasProducer(false),
	fConnected(false),
	fRunning(false)
{
	fRoster = BMediaRoster::Roster();
	fConsumer = new FlickConsumer(target);
	if (fRoster != NULL)
		fRoster->RegisterNode(fConsumer);
}


FlickCamera::~FlickCamera()
{
	Close();
	if (fRoster != NULL && fConsumer != NULL)
		fRoster->ReleaseNode(fConsumer->Node());
}


/*static*/ void
FlickCamera::List(std::vector<flick_camera_info>& cameras)
{
	cameras.clear();

	BMediaRoster* roster = BMediaRoster::Roster();
	if (roster == NULL)
		return;

	media_format format;
	format.type = B_MEDIA_RAW_VIDEO;
	format.u.raw_video = media_raw_video_format::wildcard;

	const int32 kMax = 16;
	dormant_node_info nodes[kMax];
	int32 count = kMax;
	if (roster->GetDormantNodes(nodes, &count, NULL, &format, NULL,
			B_BUFFER_PRODUCER | B_PHYSICAL_INPUT) != B_OK) {
		return;
	}

	for (int32 i = 0; i < count; i++) {
		flick_camera_info info;
		info.node = nodes[i];
		strlcpy(info.name, nodes[i].name, sizeof(info.name));
		cameras.push_back(info);
	}
}


status_t
FlickCamera::Open(const flick_camera_info& camera)
{
	Close();
	if (fRoster == NULL)
		return B_NO_INIT;

	status_t status = fRoster->InstantiateDormantNode(camera.node, &fProducer);
	if (status != B_OK) {
		// A camera can only be taken once. If it is still there from an
		// earlier run, use that node.
		status = _FindLiveNode(camera);
		if (status != B_OK)
			return status;
	}

	fHasProducer = true;
	return B_OK;
}


void
FlickCamera::Close()
{
	Stop();
	if (fHasProducer && fRoster != NULL)
		fRoster->ReleaseNode(fProducer);
	fHasProducer = false;
}


/*!	Asks the camera for its sizes. A camera node that publishes them as a
	B_RESOLUTION parameter is taken by its word; for any other, the common
	sizes are tried one by one.
*/
int
FlickCamera::FindSizes(flick_size* sizes, int maxCount)
{
	int count = _SizesFromParameters(sizes, maxCount);
	if (count == 0)
		count = _SizesByTrying(sizes, maxCount);
	return flick_sort_sizes(sizes, count);
}


int
FlickCamera::_SizesFromParameters(flick_size* sizes, int maxCount)
{
	BParameterWeb* web = NULL;
	if (fRoster->GetParameterWebFor(fProducer, &web) != B_OK || web == NULL)
		return 0;

	int count = 0;
	BDiscreteParameter* resolution = _FindResolution(web);
	for (int32 i = 0; resolution != NULL && i < resolution->CountItems()
			&& count < maxCount; i++) {
		if (flick_size_from_parameter(resolution->ItemValueAt(i),
				&sizes[count])) {
			count++;
		}
	}
	delete web;
	return count;
}


BDiscreteParameter*
FlickCamera::_FindResolution(BParameterWeb* web)
{
	for (int32 i = 0; i < web->CountParameters(); i++) {
		BParameter* parameter = web->ParameterAt(i);
		if (parameter->Type() == BParameter::B_DISCRETE_PARAMETER
			&& strcmp(parameter->Kind(), B_RESOLUTION) == 0) {
			return static_cast<BDiscreteParameter*>(parameter);
		}
	}
	return NULL;
}


int
FlickCamera::_SizesByTrying(flick_size* sizes, int maxCount)
{
	int count = 0;
	for (int i = 0; i < kCandidateCount && count < maxCount; i++) {
		// A connection that the camera accepts is enough to know the size; it
		// is taken down at once, nothing is started.
		if (_Connect(kCandidateSizes[i].width, kCandidateSizes[i].height)
				== B_OK) {
			if (fConsumer->Width() == kCandidateSizes[i].width
				&& fConsumer->Height() == kCandidateSizes[i].height) {
				sizes[count++] = kCandidateSizes[i];
			}
			_Disconnect();
		}
	}
	return count;
}


status_t
FlickCamera::Start(uint32 width, uint32 height)
{
	Stop();
	if (!fHasProducer)
		return B_NO_INIT;

	status_t status = _Connect(width, height);
	if (status != B_OK)
		return status;

	media_node timeSourceNode;
	status = fRoster->GetTimeSource(&timeSourceNode);
	if (status != B_OK)
		return status;

	status = fRoster->SetTimeSourceFor(fProducer.node, timeSourceNode.node);
	if (status != B_OK)
		return status;
	status = fRoster->SetTimeSourceFor(fConsumer->ID(), timeSourceNode.node);
	if (status != B_OK)
		return status;

	bigtime_t latency = 0;
	fRoster->GetLatencyFor(fProducer, &latency);
	fRoster->SetProducerRunModeDelay(fProducer, latency);

	bigtime_t initialLatency = 0;
	if (fRoster->GetInitialLatencyFor(fProducer, &initialLatency) < B_OK)
		initialLatency = 0;
	initialLatency += estimate_max_scheduling_latency();

	BTimeSource* timeSource = fRoster->MakeTimeSourceFor(fProducer);
	if (timeSource == NULL)
		return B_ERROR;

	// Without a sound card the system time source may not run.
	bigtime_t real = BTimeSource::RealTime();
	if (!timeSource->IsRunning()) {
		fRoster->StartTimeSource(timeSourceNode, real);
		fRoster->SeekTimeSource(timeSourceNode, 0, real);
	}
	bigtime_t performance
		= timeSource->PerformanceTimeFor(real + latency + initialLatency);
	timeSource->Release();

	status = fRoster->StartNode(fProducer, performance);
	if (status != B_OK)
		return status;
	status = fRoster->StartNode(fConsumer->Node(), performance);
	if (status != B_OK)
		return status;

	fRunning = true;
	return B_OK;
}


void
FlickCamera::Stop()
{
	if (fRoster == NULL)
		return;

	if (fRunning)
		fRoster->StopNode(fConsumer->Node(), 0, true);
	fRunning = false;
	_Disconnect();
}


status_t
FlickCamera::_FindLiveNode(const flick_camera_info& camera)
{
	const int32 kMax = 32;
	live_node_info nodes[kMax];
	int32 count = kMax;
	if (fRoster->GetLiveNodes(nodes, &count) != B_OK)
		return B_ERROR;

	for (int32 i = 0; i < count; i++) {
		fprintf(stderr, "Flick: live node %d \"%s\" (looking for \"%s\")\n",
			(int)i, nodes[i].name, camera.name);
		if (strcmp(nodes[i].name, camera.name) == 0)
			return fRoster->GetNodeFor(nodes[i].node.node, &fProducer);
	}
	fprintf(stderr, "Flick: no live node found (%d nodes)\n", (int)count);
	return B_ENTRY_NOT_FOUND;
}


status_t
FlickCamera::_Connect(uint32 width, uint32 height)
{
	if (!fHasProducer || fRoster == NULL)
		return B_NO_INIT;
	if (fConnected)
		_Disconnect();

	int32 count = 0;
	status_t status = fRoster->GetFreeOutputsFor(fProducer, &fOutput, 1,
		&count, B_MEDIA_RAW_VIDEO);
	if (status != B_OK || count < 1)
		return B_RESOURCE_UNAVAILABLE;

	count = 0;
	status = fRoster->GetFreeInputsFor(fConsumer->Node(), &fInput, 1, &count,
		B_MEDIA_RAW_VIDEO);
	if (status != B_OK || count < 1)
		return B_RESOURCE_UNAVAILABLE;

	media_format format;
	format.type = B_MEDIA_RAW_VIDEO;
	format.u.raw_video = media_raw_video_format::wildcard;
	format.u.raw_video.display.format = B_RGB32;
	format.u.raw_video.display.line_width = width;
	format.u.raw_video.display.line_count = height;

	status = fRoster->Connect(fOutput.source, fInput.destination, &format,
		&fOutput, &fInput);
	if (status != B_OK)
		return status;

	fConnected = true;
	return B_OK;
}


void
FlickCamera::_Disconnect()
{
	if (!fConnected || fRoster == NULL)
		return;

	fRoster->Disconnect(fOutput.node.node, fOutput.source,
		fInput.node.node, fInput.destination);
	fConnected = false;
}
