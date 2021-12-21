#include "task/live_view_task.h"

#include "logging.h"

void LiveViewTask::Run()
{
    PrepareBuffer();
    StartAcqusition();
    is_running = true;

    try {
        for (;;) {
            ImageData frame = GetFrame();
            if (frame.empty()) {
                is_running = false;
                data_manager->SetLiveViewFrame(frame);
                return;
            }
            data_manager->SetLiveViewFrame(frame);
        }
    } catch (std::exception &e) {
        StopAcqusition();
        is_running = false;
        data_manager->SetLiveViewFrame(ImageData());
    }
}

void LiveViewTask::Stop()
{
    StopAcqusition();
}

void LiveViewTask::PrepareBuffer()
{
    int n_buffer_frames = 2;

    utils::StopWatch sw;
    if (dcam->BufferAllocated() < n_buffer_frames) {
        if (dcam->BufferAllocated() > 0) {
            LOG_DEBUG("[{}] Releasing Buffer (n_frame={})...", task_name,
                      dcam->BufferAllocated());
            sw.Reset();
            dcam->ReleaseBuffer();
            LOG_DEBUG("[{}] Buffer released [{:.1f} ms]", task_name,
                      sw.Milliseconds());
        }
        sw.Reset();
        dcam->AllocBuffer(n_buffer_frames);
        LOG_DEBUG("[{}] Buffer allocated (n_frame={}) [{:.1f} ms]", task_name,
                  n_buffer_frames, sw.Milliseconds());
    } else {
        LOG_DEBUG("[{}] Using existing buffer (n_frame={})", task_name,
                  n_buffer_frames);
    }
}

Status LiveViewTask::StartAcqusition()
{
    utils::StopWatch sw;
    StatusOr<std::string> trigger_source = dcam->GetProperty("TRIGGER SOURCE");
    if (!trigger_source.ok()) {
        return trigger_source.status();
    }

    if (trigger_source.value() != "INTERNAL") {
        Status status = dcam->SetProperty("TRIGGER SOURCE", "INTERNAL");
        if (!status.ok()) {
            return status;
        }
        LOG_DEBUG("[{}] Internal trigger enabled [{:.1f} ms]", task_name,
                  sw.Milliseconds());
    } else {
        LOG_DEBUG("[{}] Internal trigger already enabled [{:.1f} ms]",
                  task_name, sw.Milliseconds());
    }

    sw.Reset();
    Status status = dcam->StartContinousAcquisition();
    if (!status.ok()) {
        return status;
    }
    LOG_DEBUG("[{}] Continous acquisition started [{:.1f} ms]", task_name,
              sw.Milliseconds());
    return absl::OkStatus();
}

ImageData LiveViewTask::GetFrame()
{
    Status status = dcam->WaitFrameReady(1000);
    if (!status.ok()) {
        LOG_DEBUG("[{}] WaitFrameReady returned false, which indicates ABORT",
                  task_name);
        return ImageData();
    }

    // Get latest frame
    StatusOr<ImageData> frame = dcam->GetFrame(-1);
    if (!frame.ok()) {
        throw std::runtime_error(
            "GetFrame returned empty ImageData without throwing an exception");
    }

    return frame.value();
}

Status LiveViewTask::StopAcqusition()
{
    utils::StopWatch sw;
    Status status = dcam->StopAcquisition();
    if (!status.ok()) {
        return status;
    }
    LOG_DEBUG("[{}] Acquisition stopped [{:.1f} ms]", task_name,
              sw.Milliseconds());
    return absl::OkStatus();
}
