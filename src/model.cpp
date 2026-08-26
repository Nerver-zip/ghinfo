#include "ghinfo/model.hpp"

namespace ghinfo {

std::string to_string(RunStatus status) {
    switch (status) {
    case RunStatus::queued:
        return "queued";
    case RunStatus::in_progress:
        return "in_progress";
    case RunStatus::completed:
        return "completed";
    case RunStatus::unknown:
        return "unknown";
    }
    return "unknown";
}

std::string to_string(Conclusion conclusion) {
    switch (conclusion) {
    case Conclusion::success:
        return "success";
    case Conclusion::failure:
        return "failure";
    case Conclusion::cancelled:
        return "cancelled";
    case Conclusion::skipped:
        return "skipped";
    case Conclusion::timed_out:
        return "timed_out";
    case Conclusion::neutral:
        return "neutral";
    case Conclusion::action_required:
        return "action_required";
    case Conclusion::unknown:
        return "unknown";
    }
    return "unknown";
}

} // namespace ghinfo
