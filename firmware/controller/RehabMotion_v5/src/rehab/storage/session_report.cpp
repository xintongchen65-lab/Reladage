#include "session_report.h"
#include <algorithm>
#include <cstdio>
namespace rehab {
void SessionAccumulator::reset(){ total_=accepted_=rejected_=0; romSum_=maxRom_=qualitySum_=0; }
void SessionAccumulator::add(const ExerciseFeedback& f){ if(!f.repCompleted) return; ++total_; if(f.repAccepted) ++accepted_; else ++rejected_; romSum_+=f.peakRomDeg; maxRom_=std::max(maxRom_,f.peakRomDeg); qualitySum_+=f.qualityScore; }
ReportSummary SessionAccumulator::summary() const { ReportSummary r{}; r.totalReps=total_;r.acceptedReps=accepted_;r.rejectedReps=rejected_;r.maxRomDeg=maxRom_; if(total_){r.meanRomDeg=romSum_/total_;r.meanQualityScore=qualitySum_/total_;r.passRatePct=100.0f*accepted_/total_;} return r; }
std::string SessionAccumulator::summaryJson() const { auto r=summary(); char b[320]; std::snprintf(b,sizeof(b),"{\"total_reps\":%lu,\"accepted_reps\":%lu,\"rejected_reps\":%lu,\"max_rom_deg\":%.1f,\"mean_rom_deg\":%.1f,\"mean_quality_score\":%.1f,\"pass_rate_pct\":%.1f}",(unsigned long)r.totalReps,(unsigned long)r.acceptedReps,(unsigned long)r.rejectedReps,r.maxRomDeg,r.meanRomDeg,r.meanQualityScore,r.passRatePct); return b; }
}
