#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <SdFat.h>
#include <math.h>
#include <string.h>
#include "pin_config.h"
#include "training_logic.h"
#include "real_imu_abcde.h"
#include "gyro_elbow_k11.h"

class SDLogger {
public:
  bool begin(SPIClass &sharedSPI) {
    spiBus = &sharedSPI;
    prepareBusPins();

    Serial.println("Initializing RehabMotion V4 A2 K2.3 SD logger...");
    Serial.print("SD pins: CS=");
    Serial.print(PIN_SD_CS);
    Serial.print(" SCK=");
    Serial.print(PIN_SPI_SCK);
    Serial.print(" MOSI=");
    Serial.print(PIN_SPI_MOSI);
    Serial.print(" MISO=");
    Serial.println(PIN_SPI_MISO);

    // K2.3: give the card/socket/power rail a short settling interval, then use
    // multi-round 4 MHz -> 1 MHz recovery instead of permanently giving up
    // after one fast + one slow attempt.
    delay(120);
    return ensureReady("BOOT", 3);
  }

  // K2.3 public recovery hook. Safe to call before a training session.
  // It never touches the motion algorithm; it only re-arms the shared SPI bus
  // and remounts the card/filesystem.
  bool ensureReady(const char *reason = "MANUAL", int rounds = 3) {
    if (ready) return true;
    if (spiBus == nullptr) {
      Serial.print("SD_RECOVERY_ABORT reason=");
      Serial.print(reason ? reason : "");
      Serial.println(" cause=no_spi_bus");
      return false;
    }

    if (rounds < 1) rounds = 1;
    if (rounds > 5) rounds = 5;

    active = false;
    if (logFile.isOpen()) logFile.close();
    currentFileName[0] = '\0';

    for (int round = 1; round <= rounds; ++round) {
      Serial.print("SD_RECOVERY_ROUND reason=");
      Serial.print(reason ? reason : "");
      Serial.print(" round=");
      Serial.print(round);
      Serial.print('/');
      Serial.println(rounds);

      if (tryMount(4, reason, round)) {
        ready = true;
        Serial.print("SD_RECOVERY_OK reason=");
        Serial.print(reason ? reason : "");
        Serial.println(" speed_mhz=4");
        return true;
      }

      // Explicitly re-arm the SPI pins before the slower retry. This mirrors
      // the path that has already been used successfully in earlier builds.
      prepareBusPins();
      spiBus->begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI, PIN_SD_CS);
      delay(60UL * (unsigned long)round);

      if (tryMount(1, reason, round)) {
        ready = true;
        Serial.print("SD_RECOVERY_OK reason=");
        Serial.print(reason ? reason : "");
        Serial.println(" speed_mhz=1");
        return true;
      }

      ready = false;
      prepareBusPins();
      delay(120UL * (unsigned long)round);
    }

    Serial.print("SD_RECOVERY_FAILED reason=");
    Serial.print(reason ? reason : "");
    Serial.print(" rounds=");
    Serial.println(rounds);
    return false;
  }

  bool startSession() {
    if (active) {
      endSession("RESTART");
    }
    if (logFile.isOpen()) logFile.close();
    active = false;

    // K2.3: a boot-time miss no longer disables SD for the whole power cycle.
    // Every real training session gets a fresh mount opportunity.
    if (!ready) {
      Serial.println("SD_SESSION_RECOVERY_REQUEST: logger_not_ready");
      if (!ensureReady("TRAIN_START", 3)) {
        Serial.println("SD_SESSION_START_FAIL: recovery_failed_before_open");
        return false;
      }
    }

    bool opened = false;
    for (int attempt = 1; attempt <= 5 && !opened; attempt++) {
      prepareBusPins();
      delay(15UL * (unsigned long)attempt);

      if (!chooseNextFileName()) {
        Serial.println("SD_SESSION_FILENAME_FAIL: remount_and_retry");
        ready = false;
        if (!ensureReady("FILENAME", 2)) return false;
        continue;
      }

      bool existedBefore = sd.exists(currentFileName);
      opened = logFile.open(currentFileName, O_RDWR | O_CREAT | O_TRUNC);
      if (opened) break;

      bool existsAfter = sd.exists(currentFileName);
      Serial.print("SD_SESSION_OPEN_FAIL attempt="); Serial.print(attempt);
      Serial.print(" file="); Serial.print(currentFileName);
      Serial.print(" existed_before="); Serial.print(existedBefore ? 1 : 0);
      Serial.print(" exists_after="); Serial.print(existsAfter ? 1 : 0);
      Serial.print(" TFT_CS="); Serial.print(digitalRead(PIN_TFT_CS));
      Serial.print(" SD_CS="); Serial.println(digitalRead(PIN_SD_CS));
      if (logFile.isOpen()) logFile.close();
      currentFileName[0] = '\0';

      // After an actual open failure, assume the mount/shared-SPI state may
      // be stale and force a remount before trying another filename.
      ready = false;
      if (!ensureReady("OPEN_FAIL", 2)) {
        Serial.println("SD_SESSION_OPEN_RECOVERY_FAIL");
        return false;
      }
      delay(35UL * (unsigned long)attempt);
    }

    if (!opened) {
      Serial.println("SD_SESSION_START_FAIL: open_failed_after_5_attempts");
      return false;
    }

    Serial.print("SD_SESSION_OPEN_OK file=");
    Serial.println(currentFileName);

    logFile.println("# RehabMotion V4.4.29 ABCDE; A/B=left pair, C/D=right pair, E=waist/abdomen torso reference; ROM=per-sensor semantic hinge projection difference; P=max(E-gyro-turn-compensated B forearm body-plane,A side,K11 hinge); E Euler yaw/BODY_AZIMUTH/raw B side diagnostic only; E follows deliberate body turns for P and is never used for elbow ROM; SD self-recovery active");
    logFile.print("# sample_interval_ms=");
    logFile.println(LOG_INTERVAL_MS);
    logFile.println("# joint_angle_source=per_sensor_semantic_frame_hinge_projection_difference_integral");
    logFile.println("# k11_calibration=one_time_bias_plus_one_slow_joint_motion; no_per_rep_recalibration_or_wait_gate");
    logFile.println("# sd_policy=boot_multiround_4mhz_1mhz_retry_plus_train_start_recovery_plus_open_failure_remount");
    logFile.println("# quality=independent_ROM+sagittal_plane+upper_arm+torso; multi_issue_preserved; max_retry_per_slot=1; thresholds_are_engineering_tuning_values");
    logFile.println(
      "time_ms,seq,timestamp_ms,mode,exercise,"
      "left_angle_deg,right_angle_deg,left_rom_deg,right_rom_deg,lr_rom_diff_deg,"
      "left_count,right_count,target_count,completion_percent,training_state,rep_event,"
      "left_speed_deg_s,right_speed_deg_s,"
      "left_joint_raw_deg,right_joint_raw_deg,left_rep_max_deg,right_rep_max_deg,left_state,right_state,"
      "left_last_rep_peak_deg,left_current_rep_upper_arm_dev_max_deg,left_last_rep_upper_arm_dev_max_deg,"
      "left_last_rep_upper_arm_dev_mean_deg,left_last_rep_duration_ms,left_last_rep_smoothness,left_last_rep_max_speed_deg_s,"
      "left_last_rep_quality,left_rep_rom_low,left_rep_upper_arm_excess,left_rep_movement_unstable,"
      "left_baseline_rep_count,left_baseline_ready,left_baseline_upper_mean_deg,left_baseline_upper_peak_deg,"
      "left_upper_mean_limit_deg,left_upper_peak_limit_deg,left_fast_duration_limit_ms,left_smoothness_limit,"
      "left_attempt_good_count,left_attempt_issue_count,"
      "pair_skew_ms,sync_accepted,sync_dropped_skew,"
      "candidate_Ainv_B_deg,candidate_B_Ainv_deg,candidate_world_delta_deg,candidate_body_delta_deg,"
      "imu_a_ms,imu_b_ms,imu_a_packet,imu_b_packet,"
      "imu_a_roll,imu_a_pitch,imu_a_yaw,imu_b_roll,imu_b_pitch,imu_b_yaw,"
      "k11_phase,k11_cal_active,k11_training_ready,"
      "k11_axis_x,k11_axis_y,k11_axis_z,k11_axis_dominance,"
      "k11_cal_peak_deg,k11_cal_angle_deg,k11_extension_excursion_deg,k11_extension_speed_deg_s,"
      "k11_bias_samples_a,k11_bias_samples_b,k11_processed_a,k11_processed_b,k11_rejected_dt_a,k11_rejected_dt_b,k11_bridged_gap_a,k11_bridged_gap_b,"
      "upper_arm_rel_torso_current_deg,torso_deviation_from_group_start_deg,upper_elevation_from_neutral_down_deg,"
      "fore_elevation_from_neutral_down_deg,elbow_segment_proxy_deg,torso_reference_ready,"
      "body_front_ready,fore_sagittal_plane_deviation_deg,fore_front_component,fore_side_component,"
      "left_current_rep_plane_max_deg,left_last_rep_plane_max_deg,left_current_rep_torso_max_deg,left_last_rep_torso_max_deg,"
      "left_last_plane_deviation,left_last_torso_compensation,"
      "left_current_upper_raw_diag_max_deg,left_last_upper_raw_diag_max_deg,left_current_plane_raw_diag_max_deg,left_last_plane_raw_diag_max_deg,"
      "left_current_torso_raw_diag_max_deg,left_last_torso_raw_diag_max_deg,left_last_plane_source,"
      "left_last_plane_body_azimuth_eval_max_deg,left_last_plane_upper_side_eval_max_deg,left_last_plane_forearm_motion_eval_max_deg,left_last_plane_hinge_eval_max_deg"
    );
    logFile.sync();

    sessionStartMs = millis();
    lastLogMs = 0;
    lastSyncMs = 0;
    sampleSeq = 0;
    active = true;

    Serial.print("SD session started: ");
    Serial.println(currentFileName);
    return true;
  }

  void update(
    float leftSignedAngle,
    float leftAngle,
    const TrainingData &leftData,
    float rightSignedAngle,
    float rightAngle,
    const TrainingData &rightData,
    const char *repEvent,
    float leftSpeedDegS,
    float rightSpeedDegS,
    const char *mode,
    const char *exercise
  ) {
    if (!ready || !active || !logFile.isOpen()) {
      return;
    }

    unsigned long now = millis();

    if (now - lastLogMs >= LOG_INTERVAL_MS) {
      lastLogMs = now;
      writeSample(leftSignedAngle, leftAngle, leftData, rightSignedAngle, rightAngle, rightData, repEvent, leftSpeedDegS, rightSpeedDegS, mode, exercise);
    }

    if (now - lastSyncMs >= SYNC_INTERVAL_MS) {
      lastSyncMs = now;
      logFile.sync();
    }
  }

  void logNow(
    float leftSignedAngle,
    float leftAngle,
    const TrainingData &leftData,
    float rightSignedAngle,
    float rightAngle,
    const TrainingData &rightData,
    const char *repEvent,
    float leftSpeedDegS,
    float rightSpeedDegS,
    const char *mode,
    const char *exercise
  ) {
    if (!ready || !active || !logFile.isOpen()) {
      return;
    }

    writeSample(leftSignedAngle, leftAngle, leftData, rightSignedAngle, rightAngle, rightData, repEvent, leftSpeedDegS, rightSpeedDegS, mode, exercise);
    logFile.sync();
  }


  void logSummary(const char *summary) {
    if (!ready || !active || !logFile.isOpen()) return;
    logFile.print("# V4_2_SUMMARY,");
    logFile.println(summary ? summary : "");
    logFile.sync();
    Serial.print("SD V4 A2 summary: ");
    Serial.println(summary ? summary : "");
  }

  void logMarker(const char *marker) {
    if (!ready || !active || !logFile.isOpen()) {
      return;
    }

    logFile.print("# EVENT,");
    logFile.print(marker);
    logFile.print(",elapsed_ms=");
    logFile.println(millis() - sessionStartMs);
    logFile.sync();

    Serial.print("SD marker: ");
    Serial.println(marker);
  }

  void endSession(const char *reason = "END") {
    if (!active || !logFile.isOpen()) {
      active = false;
      return;
    }

    logFile.print("# END,reason=");
    logFile.print(reason);
    logFile.print(",elapsed_ms=");
    logFile.println(millis() - sessionStartMs);

    logFile.sync();
    logFile.close();
    active = false;

    Serial.print("SD session closed: ");
    Serial.print(currentFileName);
    Serial.print(" | reason=");
    Serial.println(reason);
  }

  bool isReady() const {
    return ready;
  }

  bool isActive() const {
    return active;
  }

  const char *fileName() const {
    return currentFileName;
  }

private:
  SPIClass *spiBus = nullptr;

  void prepareBusPins() {
    pinMode(PIN_TFT_CS, OUTPUT);
    digitalWrite(PIN_TFT_CS, HIGH);
    pinMode(PIN_SD_CS, OUTPUT);
    digitalWrite(PIN_SD_CS, HIGH);
  }

  bool tryMount(uint8_t mhz, const char *reason, int round) {
    prepareBusPins();
    SdSpiConfig cfg(PIN_SD_CS, SHARED_SPI, SD_SCK_MHZ(mhz), spiBus);
    if (sd.begin(cfg)) {
      Serial.print("SD_MOUNT_OK reason=");
      Serial.print(reason ? reason : "");
      Serial.print(" round="); Serial.print(round);
      Serial.print(" speed_mhz="); Serial.println(mhz);
      return true;
    }

    Serial.print("SD_MOUNT_FAIL reason=");
    Serial.print(reason ? reason : "");
    Serial.print(" round="); Serial.print(round);
    Serial.print(" speed_mhz="); Serial.println(mhz);
    sd.initErrorPrint(&Serial);
    return false;
  }

  static constexpr unsigned long LOG_INTERVAL_MS = 100;   // 10 Hz for current real-IMU test
  static constexpr unsigned long SYNC_INTERVAL_MS = 1000; // 1 s

  SdFs sd;
  FsFile logFile;

  bool ready = false;
  bool active = false;

  unsigned long sessionStartMs = 0;
  unsigned long lastLogMs = 0;
  unsigned long lastSyncMs = 0;

  char currentFileName[32] = "";
  uint32_t sampleSeq = 0;

  bool chooseNextFileName() {
    for (int i = 1; i <= 999; i++) {
      snprintf(
        currentFileName,
        sizeof(currentFileName),
        "session_%03d.csv",
        i
      );

      if (!sd.exists(currentFileName)) {
        return true;
      }
    }

    currentFileName[0] = '\0';
    return false;
  }

  void writeSample(
    float leftSignedAngle,
    float leftAngle,
    const TrainingData &leftData,
    float rightSignedAngle,
    float rightAngle,
    const TrainingData &rightData,
    const char *repEvent,
    float leftSpeedDegS,
    float rightSpeedDegS,
    const char *mode,
    const char *exercise
  ) {
    unsigned long timestampMs = millis();
    unsigned long elapsed = timestampMs - sessionStartMs;
    float lrDiff = fabsf(leftData.maxAngle - rightData.maxAngle);
    const char *trainingState = aggregateState(leftData, rightData);
    int completionPercent = computeCompletionPercent(leftData, rightData);

    // When the group is paused/stopped/finished, game-facing speed should be 0.
    if (strcmp(trainingState, "RUNNING") != 0) {
      leftSpeedDegS = 0.0f;
      rightSpeedDegS = 0.0f;
    }

    logFile.print(elapsed);
    logFile.print(',');
    logFile.print(sampleSeq++);
    logFile.print(',');
    logFile.print(timestampMs);
    logFile.print(',');
    logFile.print(mode ? mode : "");
    logFile.print(',');
    logFile.print(exercise ? exercise : "");
    logFile.print(',');
    logFile.print(leftData.currentAngle, 2);
    logFile.print(',');
    logFile.print(rightData.currentAngle, 2);
    logFile.print(',');
    logFile.print(leftData.maxAngle, 2);
    logFile.print(',');
    logFile.print(rightData.maxAngle, 2);
    logFile.print(',');
    logFile.print(lrDiff, 2);
    logFile.print(',');
    logFile.print(leftData.currentCount);
    logFile.print(',');
    logFile.print(rightData.currentCount);
    logFile.print(',');
    logFile.print(leftData.targetCount);
    logFile.print(',');
    logFile.print(completionPercent);
    logFile.print(',');
    logFile.print(trainingState);
    logFile.print(',');
    logFile.print(repEvent ? repEvent : "none");
    logFile.print(',');
    logFile.print(leftSpeedDegS, 2);
    logFile.print(',');
    logFile.print(rightSpeedDegS, 2);
    logFile.print(',');
    logFile.print(leftSignedAngle, 2);
    logFile.print(',');
    logFile.print(rightSignedAngle, 2);
    logFile.print(',');
    logFile.print(leftData.currentRepMaxAngle, 2);
    logFile.print(',');
    logFile.print(rightData.currentRepMaxAngle, 2);
    logFile.print(',');
    logFile.print(stateName(leftData.state));
    logFile.print(',');
    logFile.print(stateName(rightData.state));
    logFile.print(',');
    logFile.print(leftData.lastCompletedRepPeakAngle, 2);
    logFile.print(',');
    logFile.print(leftData.currentRepEvalMaxUpperArmDev, 2);
    logFile.print(',');
    logFile.print(leftData.lastCompletedRepEvalMaxUpperArmDev, 2);
    logFile.print(',');
    logFile.print(leftData.lastCompletedRepMeanUpperArmDev, 2);
    logFile.print(',');
    logFile.print(leftData.lastCompletedRepDurationMs);
    logFile.print(',');
    logFile.print(leftData.lastCompletedRepSmoothness, 2);
    logFile.print(',');
    logFile.print(leftData.lastCompletedRepMaxSpeed, 2);
    logFile.print(',');
    logFile.print(leftData.completedMotionCount <= 0 ? "NONE" : repQualityCodeName(leftData.lastRepQuality));
    logFile.print(',');
    logFile.print(leftData.lastRomLow ? 1 : 0);
    logFile.print(',');
    logFile.print(leftData.lastUpperArmExcess ? 1 : 0);
    logFile.print(',');
    logFile.print(leftData.lastMovementUnstable ? 1 : 0);
    logFile.print(',');
    logFile.print(leftData.baselineRepCount);
    logFile.print(',');
    logFile.print(leftData.baselineReady ? 1 : 0);
    logFile.print(',');
    logFile.print(leftData.baselineUpperMean, 2);
    logFile.print(',');
    logFile.print(leftData.baselineUpperPeak, 2);
    logFile.print(',');
    logFile.print(leftData.upperMeanLimit, 2);
    logFile.print(',');
    logFile.print(leftData.upperPeakLimit, 2);
    logFile.print(',');
    logFile.print(leftData.fastDurationLimitMs, 1);
    logFile.print(',');
    logFile.print(leftData.smoothnessLimit, 2);
    logFile.print(',');
    logFile.print(leftData.goodRepCount);
    logFile.print(',');
    logFile.print(leftData.issueRepCount);
    logFile.print(',');
    logFile.print(wt901LeftPairSkewMs());
    logFile.print(',');
    logFile.print(wt901LeftSyncAcceptedCount());
    logFile.print(',');
    logFile.print(wt901LeftSyncDroppedSkewCount());
    logFile.print(',');
    logFile.print(wt901LeftCandidateAinvBDeg(), 2);
    logFile.print(',');
    logFile.print(wt901LeftCandidateBAinvDeg(), 2);
    logFile.print(',');
    logFile.print(wt901LeftCandidateWorldDeltaDeg(), 2);
    logFile.print(',');
    logFile.print(wt901LeftCandidateBodyDeltaDeg(), 2);
    logFile.print(',');
    logFile.print(wt901ImuSampleMs(0));
    logFile.print(',');
    logFile.print(wt901ImuSampleMs(1));
    logFile.print(',');
    logFile.print(wt901ImuPacketCount(0));
    logFile.print(',');
    logFile.print(wt901ImuPacketCount(1));
    logFile.print(',');
    logFile.print(wt901ImuRollDeg(0), 2);
    logFile.print(',');
    logFile.print(wt901ImuPitchDeg(0), 2);
    logFile.print(',');
    logFile.print(wt901ImuYawDeg(0), 2);
    logFile.print(',');
    logFile.print(wt901ImuRollDeg(1), 2);
    logFile.print(',');
    logFile.print(wt901ImuPitchDeg(1), 2);
    logFile.print(',');
    logFile.print(wt901ImuYawDeg(1), 2);
    logFile.print(',');
    logFile.print(wt901K11PhaseName());
    logFile.print(',');
    logFile.print(wt901K11CalibrationActive() ? 1 : 0);
    logFile.print(',');
    logFile.print(wt901K11Ready() ? 1 : 0);
    logFile.print(',');
    logFile.print(wt901K11AxisX(), 5);
    logFile.print(',');
    logFile.print(wt901K11AxisY(), 5);
    logFile.print(',');
    logFile.print(wt901K11AxisZ(), 5);
    logFile.print(',');
    logFile.print(wt901K11AxisDominance(), 4);
    logFile.print(',');
    logFile.print(wt901K11PeakDeg(), 2);
    logFile.print(',');
    logFile.print(wt901K11CalAngleDeg(), 2);
    logFile.print(',');
    logFile.print(wt901K11ElbowAngleDeg(), 2);
    logFile.print(',');
    logFile.print(wt901K11ElbowSpeedDegS(), 2);
    logFile.print(',');
    logFile.print(wt901K11BiasSamplesA());
    logFile.print(',');
    logFile.print(wt901K11BiasSamplesB());
    logFile.print(',');
    logFile.print(wt901K11ProcessedA());
    logFile.print(',');
    logFile.print(wt901K11ProcessedB());
    logFile.print(',');
    logFile.print(wt901K11RejectedDtA());
    logFile.print(',');
    logFile.print(wt901K11RejectedDtB());
    logFile.print(',');
    logFile.print(wt901K11BridgedGapA());
    logFile.print(',');
    logFile.print(wt901K11BridgedGapB());
    logFile.print(',');
    logFile.print(torsoRefUpperDeviationDeg(), 2);
    logFile.print(',');
    logFile.print(torsoRefTorsoDeviationDeg(), 2);
    logFile.print(',');
    logFile.print(torsoRefUpperNeutralElevationDeg(), 2);
    logFile.print(',');
    logFile.print(torsoRefForeNeutralElevationDeg(), 2);
    logFile.print(',');
    logFile.print(torsoRefElbowProxyDeg(), 2);
    logFile.print(',');
    logFile.print(torsoRef.neutralReady ? 1 : 0);
    logFile.print(',');
    logFile.print(torsoRefFrontReady() ? 1 : 0);
    logFile.print(',');
    logFile.print(torsoRefForePlaneDeviationDeg(), 2);
    logFile.print(',');
    logFile.print(torsoRefForeFrontComponent(), 4);
    logFile.print(',');
    logFile.print(torsoRefForeSideComponent(), 4);
    logFile.print(',');
    logFile.print(leftData.currentRepEvalMaxPlaneDev, 2);
    logFile.print(',');
    logFile.print(leftData.lastCompletedRepEvalMaxPlaneDev, 2);
    logFile.print(',');
    logFile.print(leftData.currentRepEvalMaxTorsoDev, 2);
    logFile.print(',');
    logFile.print(leftData.lastCompletedRepEvalMaxTorsoDev, 2);
    logFile.print(',');
    logFile.print(leftData.lastPlaneDeviation ? 1 : 0);
    logFile.print(',');
    logFile.print(leftData.lastTorsoCompensation ? 1 : 0);
    // V4.4.16 explicit diagnostics and source traceability.
    logFile.print(',');
    logFile.print(leftData.currentRepMaxUpperArmDev, 2);
    logFile.print(',');
    logFile.print(leftData.lastCompletedRepMaxUpperArmDev, 2);
    logFile.print(',');
    logFile.print(leftData.currentRepMaxPlaneDev, 2);
    logFile.print(',');
    logFile.print(leftData.lastCompletedRepMaxPlaneDev, 2);
    logFile.print(',');
    logFile.print(leftData.currentRepMaxTorsoDev, 2);
    logFile.print(',');
    logFile.print(leftData.lastCompletedRepMaxTorsoDev, 2);
    logFile.print(',');
    logFile.print(planeDeviationSourceName(leftData.lastPlaneSource));
    logFile.print(',');
    logFile.print(leftData.lastCompletedRepEvalMaxPlaneBodyAzimuth, 2);
    logFile.print(',');
    logFile.print(leftData.lastCompletedRepEvalMaxPlaneUpperSide, 2);
    logFile.print(',');
    logFile.print(leftData.lastCompletedRepEvalMaxPlaneForearmQuat, 2);
    logFile.print(',');
    logFile.println(leftData.lastCompletedRepEvalMaxPlaneHinge, 2);
  }

  const char *aggregateState(const TrainingData &leftData, const TrainingData &rightData) {
    // K11 AB-only: A/B (leftData) is the active training pair; C/D/right is intentionally unused.
    (void)rightData;
    if (leftData.currentCount >= leftData.targetCount) return "FINISHED";
    if (leftData.state == STOPPED) return "STOPPED";
    if (leftData.state == RUNNING) return "RUNNING";
    if (leftData.state == PAUSED) return "PAUSED";
    return "IDLE";
  }

  int computeCompletionPercent(const TrainingData &leftData, const TrainingData &rightData) {
    (void)rightData;
    int target = leftData.targetCount;
    if (target <= 0) return 0;
    int completed = leftData.currentCount;
    int percent = (completed * 100) / target;
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    return percent;
  }

  const char *stateName(TrainingState state) {
    switch (state) {
      case IDLE: return "IDLE";
      case RUNNING: return "RUNNING";
      case PAUSED: return "PAUSED";
      case FINISHED: return "FINISHED";
      case STOPPED: return "STOPPED";
      default: return "UNKNOWN";
    }
  }
};
