// Hearizon Sync C.R.V. - 界面文案（集中管理，便于校对）
#pragma once

#include <string>

namespace hs {
namespace str {

// 窗口与标题
extern const char* kAppTitle;
extern const char* kSidebarTitle;
extern const char* kSidebarTagline;

// 侧栏字段
extern const char* kInputLang;
extern const char* kAudioDevice;
extern const char* kTargetLang;
extern const char* kTranslateMode;
extern const char* kApiEngine;
extern const char* kAsrEngine;
extern const char* kGpuAccel;
extern const char* kAutoCorrect;
extern const char* kRefreshDevice;
extern const char* kSubtitleStyle;
extern const char* kPosition;
extern const char* kPositionBottom;
extern const char* kPositionTop;
extern const char* kTheme;

// 按钮
extern const char* kStart;
extern const char* kStop;
extern const char* kTestMic;
extern const char* kSaveRecord;
extern const char* kExportSrt;
extern const char* kClearSubtitle;
extern const char* kMinimizeToTray;
extern const char* kAbout;

// 面板
extern const char* kSourceTitle;
extern const char* kTargetTitle;

// 状态
extern const char* kNotStarted;
extern const char* kListening;
extern const char* kRecognizing;
extern const char* kTranslating;
extern const char* kStarting;
extern const char* kStopped;

// 提示
extern const char* kApiModeStarted;      // API模式启动｜<引擎>
extern const char* kCorrectConsumeTitle;
extern const char* kCorrectConsumeBody;
extern const char* kDoNotRemind;
extern const char* kConfirmOn;
extern const char* kCancel;

// 设置页
extern const char* kSettings;
extern const char* kTranslateModeDesc;
extern const char* kDeepseekKey;
extern const char* kDeepseekHint;
extern const char* kMymemoryEmail;
extern const char* kMymemoryHint;
extern const char* kRecognitionBoost;
extern const char* kHotwords;
extern const char* kHotwordsHint;
extern const char* kHotwordsExample;
extern const char* kYoudaoTitle;
extern const char* kYoudaoHint;
extern const char* kAppKey;
extern const char* kAppSecret;
extern const char* kDomain;
extern const char* kUpdateCheck;
extern const char* kSaveSettings;
extern const char* kClose;
extern const char* kVbCableTitle;
extern const char* kVbCableHint;
extern const char* kGpuTitle;
extern const char* kGpuHint;
extern const char* kGpuProbing;
extern const char* kGpuOnOk;
extern const char* kGpuOff;
extern const char* kGpuFailPrefix;
extern const char* kGpuWarnTitle;
extern const char* kGpuWarnBodyHasCard;
extern const char* kGpuInfoTitle;
extern const char* kGpuInfoBodyNoCard;

// 更新
extern const char* kSrcPlaceholder;
extern const char* kDstPlaceholder;
extern const char* kNoSubtitle;
extern const char* kLangChanged;
extern const char* kEngineChanged;
extern const char* kEngineChangedTail;
extern const char* kTrayUnavailable;
extern const char* kModelReady;
extern const char* kUpdateAvailable;
extern const char* kUpdateLater;
extern const char* kUpdateDownload;
extern const char* kUpdateSkip;
extern const char* kUpdateDownloading;
extern const char* kUpdateDone;
extern const char* kUpdateFailed;

// 关于窗口
extern const char* kAboutTagline;
extern const char* kAuthorName;
extern const char* kLicenseLine;
extern const char* kAsrLine;
extern const char* kMtLine;
extern const char* kNotMedical;

// 独家模块
extern const char* kModuleThanksTitle;
extern const char* kModuleThanksNote;
extern const char* kModulePoemTitle;
extern const char* kModuleDeveloperTitle;
extern const char* kModuleHorizon;
extern const char* kThanksFooter;
extern const char* kPoemLine1;
extern const char* kPoemLine2;
extern const char* kPoemSource;
extern const char* kAuthorSchool;
extern const char* kAuthorMajor;
extern const char* kAuthorDesc;
extern const char* kPersonalSite;
extern const char* kLinkNote;

// 托盘
extern const char* kTrayTip;
extern const char* kTrayShow;
extern const char* kTrayToggle;
extern const char* kTrayQuit;

// 错误
extern const char* kNoModel;
extern const char* kMicError;
extern const char* kNoSpeech;
extern const char* kPunctOnly;

}  // namespace str
}  // namespace hs
