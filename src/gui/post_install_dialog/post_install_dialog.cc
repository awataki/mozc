// Copyright 2010, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "gui/post_install_dialog/post_install_dialog.h"

#ifdef OS_WINDOWS
#include <windows.h>
#endif

#include <QtGui/QtGui>
#include "base/base.h"
#include "base/process.h"
#include "base/run_level.h"
#include "base/util.h"
#include "base/win_util.h"
#include "dictionary/user_dictionary_importer.h"
#include "dictionary/user_dictionary_storage.h"
#include "dictionary/user_dictionary_util.h"
#include "usage_stats/usage_stats.h"


#ifdef OS_WINDOWS
#include "win32/base/imm_util.h"
#endif

namespace mozc {
namespace gui {

PostInstallDialog::PostInstallDialog()
    : logoff_required_(false),
      storage_(
          new UserDictionaryStorage(
              UserDictionaryUtil::GetUserDictionaryFileName())) {
#ifdef OS_WINDOWS
  if (!mozc::ImeUtil::IsCuasEnabled()) {
    logoff_required_ = true;
    mozc::ImeUtil::SetCuasEnabled(true);
  }
#endif

  setupUi(this);
  setWindowFlags(Qt::WindowSystemMenuHint |
                 Qt::MSWindowsFixedSizeDialogHint |
                 Qt::WindowStaysOnTopHint);
  setWindowModality(Qt::NonModal);

  QObject::connect(logoffNowButton,
                   SIGNAL(clicked()),
                   this,
                   SLOT(OnLogoffNow()));
  QObject::connect(logoffLaterButton,
                   SIGNAL(clicked()),
                   this,
                   SLOT(OnLogoffLater()));
  QObject::connect(okButton,
                   SIGNAL(clicked()),
                   this,
                   SLOT(OnOk()));

  // We change buttons to be displayed depending on the condition this dialog
  // is launched.
  // The following table summarizes which buttons are displayed by conditions.
  //
  // ----------------------------------------------
  // |   OK   | Later  |  Now   |  help  | logoff |
  // ----------------------------------------------
  // |   D    |   N    |   N    |  true  |  true  |
  // |   D    |   N    |   N    |  true  |  false |
  // |   N    |   D    |   D    |  false |  true  |
  // |   D    |   N    |   N    |  false |  false |
  // ----------------------------------------------
  //
  // The followings are meanings of the words used in the table.
  // OK     : okButton
  // Later  : logoffLaterButton
  // Now    : logoffNowButton
  // help   : The result of IsShowHelpPageRequired()
  // logoff : The result of IsLogoffRequired()
  // N      : not displayed
  // D      : displayed
  //
  if (IsShowHelpPageRequired()) {
    usage_stats::UsageStats::IncrementCount("PostInstallShowPageRequired");
    thanksLabel->setText(tr("Thanks for installing.\n"
                            "You need to configure your computer before using "
                            "Mozc. Please follow the "
                            "instructions on the help page."));
    logoffNowButton->setVisible(false);
    logoffLaterButton->setVisible(false);
  } else {
    if (logoff_required()) {
      usage_stats::UsageStats::IncrementCount("PostInstallLogoffRequired");
      thanksLabel->setText(tr("Thanks for installing.\nYou must log off before "
                              "using Mozc."));
      // remove OK button and move the other buttons to right.
      const int rows = gridLayout->rowCount();
      const int cols = gridLayout->columnCount();
      okButton->setVisible(false);
      gridLayout->removeWidget(okButton);
      gridLayout->addWidget(logoffNowButton, rows - 1, cols - 2);
      gridLayout->addWidget(logoffLaterButton, rows - 1, cols - 1);
    } else {
      usage_stats::UsageStats::IncrementCount("PostInstallNothingRequired");
      logoffNowButton->setVisible(false);
      logoffLaterButton->setVisible(false);
      gridLayout->removeWidget(logoffNowButton);
      gridLayout->removeWidget(logoffLaterButton);
    }
  }

  // set the default state of migrateDefaultIMEUserDictionaryCheckBox
  const bool status = (!RunLevel::IsElevatedByUAC() && storage_->Lock());
  migrateDefaultIMEUserDictionaryCheckBox->setVisible(status);

  // import MS-IME by default
  migrateDefaultIMEUserDictionaryCheckBox->setChecked(true);
}

PostInstallDialog::~PostInstallDialog() {
}

bool PostInstallDialog::logoff_required() {
  return logoff_required_;
}

bool PostInstallDialog::ShowHelpPageIfRequired() {
  if (PostInstallDialog::IsShowHelpPageRequired()) {
    const char kHelpPageUrl[] =
        "http://www.google.com/support/ime/japanese/bin/answer.py?hl=jp&answer="
        "166771";
    return mozc::Process::OpenBrowser(kHelpPageUrl);
  }
  return false;
}

// NOTE(mazda): UsageStats class is not currently multi-process safe so it is
// possible that usagestats is incorrectly collected.
// For example if the user activate Mozc in another application before closing
// this dialog, the usagestats collected in the application can be overwritten
// when this dialog is closed.
// But this case is very rare since this dialog is launched immediately after
// installation.
// So we accept the potential error until this limitation is fixed.
void PostInstallDialog::OnLogoffNow() {
  usage_stats::UsageStats::IncrementCount("PostInstallLogoffNowButton");
  ApplySettings();
#ifdef OS_WINDOWS
  mozc::WinUtil::Logoff();
#else
  // not supported on Mac and Linux
#endif  // OS_WINDOWS
  done(QDialog::Accepted);
}

void PostInstallDialog::OnLogoffLater() {
  usage_stats::UsageStats::IncrementCount("PostInstallLogoffLaterButton");
  ApplySettings();
  done(QDialog::Rejected);
}

void PostInstallDialog::OnOk() {
  usage_stats::UsageStats::IncrementCount("PostInstallOkButton");
  ApplySettings();
  done(QDialog::Accepted);
}

void PostInstallDialog::reject() {
  usage_stats::UsageStats::IncrementCount("PostInstallRejectButton");
  done(QDialog::Rejected);
}

void PostInstallDialog::ApplySettings() {
#ifdef OS_WINDOWS
  if (setAsDefaultCheckBox->isChecked()) {
    usage_stats::UsageStats::IncrementCount("PostInstallSetDefault");
    ImeUtil::SetDefault();
  } else {
    usage_stats::UsageStats::IncrementCount("PostInstallNotSetDefault");
  }

  if (migrateDefaultIMEUserDictionaryCheckBox->isChecked() &&
      migrateDefaultIMEUserDictionaryCheckBox->isVisible()) {
    storage_->Load();
    // create UserDictionary if the current user dictionary is empty
    if (!storage_->Exists()) {
      const QString name = tr("User Dictionary 1");
      uint64 dic_id = 0;
      if (!storage_->CreateDictionary(name.toStdString(),
                                      &dic_id)) {
        LOG(ERROR) << "Failed to create a new dictionary.";
        return;
      }
    }

    // Import MS-IME's dictionary to an unique dictionary labeled
    // as "MS-IME"
    uint64 dic_id = 0;
    const QString name = tr("MS-IME User Dictionary");
    for (size_t i = 0; i < storage_->dictionaries_size(); ++i) {
      if (storage_->dictionaries(i).name() == name.toStdString()) {
        dic_id = storage_->dictionaries(i).id();
        break;
      }
    }

    if (dic_id == 0) {
      if (!storage_->CreateDictionary(name.toStdString(),
                                      &dic_id)) {
        LOG(ERROR) << "Failed to create a new dictionary.";
        return;
      }
    }

    UserDictionaryStorage::UserDictionary *dic =
        storage_->GetUserDictionary(dic_id);
    if (dic == NULL) {
      LOG(ERROR) << "GetUserDictionary returned NULL";
      return;
    }

    if (UserDictionaryImporter::ImportFromMSIME(dic) !=
        UserDictionaryImporter::IMPORT_NO_ERROR) {
      LOG(ERROR) << "ImportFromMSIME failed";
    }

    storage_->Save();
  }
#else
  // not supported on Mac and Linux
#endif  // OS_WINDOWS
}

bool PostInstallDialog::IsShowHelpPageRequired() {
#ifdef OS_WINDOWS
  return !ImeUtil::IsCtfmonRunning();
#else
  // not supported on Mac and Linux
  return false;
#endif  // OS_WINDOWS
}

}  // namespace gui
}  // namespace mozc
