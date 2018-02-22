/// \file
/// \author Xavier Michelon
///
/// \brief Implementation of combo import dialog
///  
/// Copyright (c) Xavier Michelon. All rights reserved.  
/// Licensed under the MIT License. See LICENSE file in the project root for full license information.  


#include "stdafx.h"
#include "ComboImportDialog.h"
#include "ComboManager.h"
#include "PreferencesManager.h"
#include "BeeftextGlobals.h"
#include "BeeftextConstants.h"


//**********************************************************************************************************************
/// \param[in] filePath The path of the file to import. Can be null
/// \param[in] parent The parent widget of the dialog
//**********************************************************************************************************************
ComboImportDialog::ComboImportDialog(QString const& filePath, QWidget* parent)
   : QDialog(parent, constants::kDefaultDialogFlags)
{
   ui_.setupUi(this);
   if (!filePath.isEmpty())
   {
      ui_.editPath->setText(QDir::toNativeSeparators(filePath));
      if (!filePath.isEmpty())
         PreferencesManager::instance().setLastComboImportExportPath(filePath);
   }
   else
      this->updateGui();
}


//**********************************************************************************************************************
/// \param[in] event The event
//**********************************************************************************************************************
void ComboImportDialog::dragEnterEvent(QDragEnterEvent* event)
{
   if (event->mimeData()->hasUrls()) // we only accept files
      event->acceptProposedAction();
}


//**********************************************************************************************************************
/// \param[in] event The event
//**********************************************************************************************************************
void ComboImportDialog::dragMoveEvent(QDragMoveEvent* event)
{
   if (event->mimeData()->hasUrls()) // we only accept files
      event->acceptProposedAction();
}


//**********************************************************************************************************************
/// \param[in] event The event
//**********************************************************************************************************************
void ComboImportDialog::dragLeaveEvent(QDragLeaveEvent* event)
{
   event->accept();
}


//**********************************************************************************************************************
/// \param[in] event The event
//**********************************************************************************************************************
void ComboImportDialog::dropEvent(QDropEvent* event)
{
   QMimeData const* mimeData = event->mimeData();
   if (!mimeData->hasUrls())
      return;
   QList<QUrl> urls = mimeData->urls();
   if (urls.size() > 0) // should always be the case
      ui_.editPath->setText(QDir::toNativeSeparators(urls[0].toLocalFile()));
   event->acceptProposedAction();
}


//**********************************************************************************************************************
// 
//**********************************************************************************************************************
void ComboImportDialog::updateGui()
{
   quint32 const conflictingNewerCount = conflictingNewerCombos_.size();
   quint32 const conflictingTotalCount = conflictingNewerCount + conflictingOlderCombos_.size();

   ui_.groupBoxConflicts->setVisible(conflictingTotalCount > 0);

   if (conflictingTotalCount)
   {
      ui_.radioSkipConflicts->setText(conflictingTotalCount > 1 ? tr("Skip %1 conflicting combos.")
         .arg(conflictingTotalCount) : tr("Skip 1 conflicting combo."));
      ui_.radioOverwrite->setText(conflictingTotalCount > 1 ? tr("Overwrite %1 conflicting combos.")
         .arg(conflictingTotalCount) : tr("Overwrite 1 conflicting combo."));
      ui_.radioImportNewer->setVisible(conflictingNewerCount);
      if (conflictingNewerCount)
         ui_.radioImportNewer->setText(conflictingNewerCount > 1 ? tr("Overwrite %1 newer conflicting combos.")
            .arg(conflictingNewerCount) : tr("Overwrite 1 newer conflicting combo.")); 
      if ((!conflictingNewerCombos_.size()) && ui_.radioImportNewer->isChecked())
      {
         QSignalBlocker b1(ui_.radioImportNewer), b2(ui_.radioSkipConflicts);
         ui_.radioSkipConflicts->setChecked(true);
      }
   }

   quint32 const importCount = this->computeTotalImportCount();
   ui_.buttonImport->setEnabled(importCount);
   ui_.labelImportCount->setVisible(importCount);
   if (importCount)
      ui_.labelImportCount->setText(importCount > 1 ? tr("%1 combos will be imported.").arg(importCount) :
         tr("1 combo will be imported."));
}


//**********************************************************************************************************************
/// \return The number of combos that will be imported based on the user choices
//**********************************************************************************************************************
qint32 ComboImportDialog::computeTotalImportCount() const
{
   qint32 result = importableCombos_.size();
   if (ui_.radioImportNewer->isChecked())
      return result + conflictingNewerCombos_.size();
   if (ui_.radioOverwrite->isChecked())
      return  result + conflictingNewerCombos_.size() + conflictingOlderCombos_.size();
   return result;
}


//**********************************************************************************************************************
/// \param[out] outFailureCount on exit, this variable contains the number of failed imports
//**********************************************************************************************************************
void ComboImportDialog::performFinalImport(qint32& outFailureCount)
{
   outFailureCount = 0;
   ComboList& comboList = ComboManager::instance().getComboListRef();
   qint32 failureCount = 0;
   for (SPCombo combo : importableCombos_)
      if (!comboList.append(combo))
         ++failureCount;

   if (ui_.radioSkipConflicts->isChecked())
      return;

   if (ui_.radioOverwrite->isChecked() && conflictingOlderCombos_.size())
      std::copy(conflictingOlderCombos_.begin(), conflictingOlderCombos_.end(),
         std::back_inserter(conflictingNewerCombos_));

   for (SPCombo combo : conflictingNewerCombos_)
   {
      ComboList::iterator it = comboList.findByKeyword(combo->keyword());
      if (it == comboList.end())
      {
         ++failureCount;
         continue;
      }
      *it = combo;
   }
}


//**********************************************************************************************************************
//
//**********************************************************************************************************************
void ComboImportDialog::onActionImport()
{
   if (!this->computeTotalImportCount())
      return;

   qint32 failureCount = 0;
   this->performFinalImport(failureCount);
   
   QString errorMsg;
   if ((!ComboManager::instance().saveComboListToFile(&errorMsg)))
      QMessageBox::critical(this, tr("Error"), errorMsg);
   
   if (failureCount)
   {
      globals::debugLog().addError(QString("%1 supposedly possible combo import failed").arg(failureCount));
      QMessageBox::critical(this, tr("Error"), tr("%1 combo(s) could not be imported.").arg(failureCount));
   }

   this->accept();
}


//**********************************************************************************************************************
//
//**********************************************************************************************************************
void ComboImportDialog::onActionCancel()
{
   this->reject();
}


//**********************************************************************************************************************
// 
//**********************************************************************************************************************
void ComboImportDialog::onActionBrowse()
{
   PreferencesManager& prefs = PreferencesManager::instance();
   QString const path = QFileDialog::getOpenFileName(this, tr("Select Combo File"), prefs.lastComboImportExportPath(),
      constants::kJsonFileDialogFilter);
   if (path.isEmpty())
      return;
   prefs.setLastComboImportExportPath(path);
   ui_.editPath->setText(QDir::toNativeSeparators(path));
}


//**********************************************************************************************************************
/// \param[in] text The new text of the edit control
//**********************************************************************************************************************
void ComboImportDialog::onEditPathTextChanged(QString const& text)
{
   importableCombos_.clear();
   conflictingOlderCombos_.clear();
   conflictingNewerCombos_.clear();

   ComboList candidateList;
   ComboList& comboList = ComboManager::instance().getComboListRef();
   if (!candidateList.load(text))
   {
      this->updateGui();
      return;
   }
   for (SPCombo const& combo : candidateList)
   {
      combo->changeUuid(); // we get new a new UUID for imported combo. UUID are intended for synchronization purposes, not import/export
      ComboList::const_iterator it = comboList.findByKeyword(combo->keyword());
      if (comboList.end() == it)
      {
         importableCombos_.append(combo);
         continue;
      }
      Combo const& existingCombo = **it;
      if (combo->lastModificationDate() > existingCombo.lastModificationDate())
         conflictingNewerCombos_.append(combo);
      else
         conflictingOlderCombos_.append(combo);
   }
   this->updateGui();
}


//**********************************************************************************************************************
// 
//**********************************************************************************************************************
void ComboImportDialog::onConflictRadioToggled(bool state)
{
   if (!state)
      return; // we are only interested in signals from the radio being checked
   this->updateGui();
}


