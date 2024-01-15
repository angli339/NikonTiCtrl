#include "task/live_view_task.h"
#include "experimentcontrol.h"

#include "logging.h"

LiveViewTask::LiveViewTask(ExperimentControl *exp)
{
    this->exp = exp;
    this->dcam = exp->Devices()->GetHamamatsuDCam();
}

bool LiveViewTask::IsRunning() { return is_running; }

void LiveViewTask::Run()
{
    Status status;
    status = PrepareBuffer();
    if (!status.ok()) {
        throw std::runtime_error(status.ToString());
    }
    status = StartAcquisition();
    if (!status.ok()) {
        throw std::runtime_error(status.ToString());
    }
    is_running = true;

    try {
        for (;;) {
            StatusOr<ImageData> frame = GetFrame();
            if (absl::IsCancelled(frame.status())) {
                is_running = false;
                exp->Images()->SetLiveViewFrame(ImageData());
                return;
            }
            if (absl::IsDataLoss(frame.status())) {
                LOG_WARN("ignore frame data loss ({})",
                         frame.status().ToString());
                continue;
            }
            if (!status.ok()) {
                throw std::runtime_error(status.ToString());
            }
            exp->Images()->SetLiveViewFrame(frame.value());
        }
    } catch (std::exception &e) {
        status = absl::InternalError(e.what());
        Status stop_acq_status = StopAcquisition();
        if (!stop_acq_status.ok()) {
            LOG_ERROR("StopAcquisition failed: {}", stop_acq_status.ToString());
        }
        is_running = false;
        exp->Images()->SetLiveViewFrame(ImageData());
        throw e;
    }
    return;
}

void LiveViewTask::Stop()
{
    Status status = StopAcquisition();
    if (!status.ok()) {
        throw std::runtime_error(status.ToString());
    }
}

Status LiveViewTask::PrepareBuffer()
{
    int n_buffer_frames = 2;

    utils::StopWatch sw;
    if (dcam->BufferAllocated() < n_buffer_frames) {
        if (dcam->BufferAllocated() > 0) {
            LOG_DEBUG("[{}] Releasing Buffer (n_frame={})...", task_name,
                      dcam->BufferAllocated());
            sw.Reset();
            Status status = dcam->ReleaseBuffer();
            if (!status.ok()) {
                LOG_ERROR("[{}] Release buffer failed: {}", task_name,
                          status.ToString());
                return status;
            }
            LOG_DEBUG("[{}] Buffer released [{:.1f} ms]", task_name,
                      sw.Milliseconds());
        }
        sw.Reset();
        Status status = dcam->AllocBuffer(n_buffer_frames);
        if (!status.ok()) {
            LOG_ERROR("[{}] alloc buffer failed: {}", task_name,
                      status.ToString());
            return status;
        }
        LOG_DEBUG("[{}] Buffer allocated (n_frame={}) [{:.1f} ms]", task_name,
                  n_buffer_frames, sw.Milliseconds());
    } else {
        LOG_DEBUG("[{}] Using existing buffer (n_frame={})", task_name,
                  n_buffer_frames);
    }
    return absl::OkStatus();
}

Status LiveViewTask::StartAcquisition()
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

StatusOr<ImageData> LiveViewTask::GetFrame()
{
    Status status = dcam->WaitFrameReady(1000);
    if (absl::IsCancelled(status) || absl::IsDataLoss(status)) {
        return status;
    }
    if (!status.ok()) {
        LOG_ERROR("[{}] WaitFrameReady failed: {}", task_name,
                  status.ToString());
        return absl::InternalError("WaitFrameReady failed: " +
                                   status.ToString());
    }

    // Get latest frame
    return dcam->GetFrame(-1);
}

Status LiveViewTask::StopAcquisition()
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
