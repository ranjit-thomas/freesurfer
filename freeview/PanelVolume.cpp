/**
 * @file  PanelVolume.cpp
 * @brief REPLACE_WITH_ONE_LINE_SHORT_DESCRIPTION
 *
 */
/*
 * Original Author: Ruopeng Wang
 * CVS Revision Info:
 *    $Author: rpwang $
 *    $Date: 2017/01/20 19:58:46 $
 *    $Revision: 1.107 $
 *
 * Copyright © 2011 The General Hospital Corporation (Boston, MA) "MGH"
 *
 * Terms and conditions for use, reproduction, distribution and contribution
 * are found in the 'FreeSurfer Software License Agreement' contained
 * in the file 'LICENSE' found in the FreeSurfer distribution, and here:
 *
 * https://surfer.nmr.mgh.harvard.edu/fswiki/FreeSurferSoftwareLicense
 *
 * Reporting: freesurfer@nmr.mgh.harvard.edu
 *
 */
#include "PanelVolume.h"
#include "ui_PanelVolume.h"
#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "LayerCollection.h"
#include "LayerMRI.h"
#include "LayerPropertyMRI.h"
#include "LayerDTI.h"
#include "LayerPropertyDTI.h"
#include "LayerVolumeTrack.h"
#include "LUTDataHolder.h"
#include "LayerSurface.h"
#include "SurfaceOverlay.h"
#include "MyUtils.h"
#include "BrushProperty.h"
#include <QToolBar>
#include <QDebug>
#include <QStringList>
#include <QFileDialog>
#include <QMessageBox>
#include <QClipboard>
#include <QMimeData>

#define FS_VOLUME_SETTING_ID    "freesurfer/volume-setting"

int ColorTableItem::SortType = ColorTableItem::ST_VALUE;
bool ColorTableItem::SortAscending = true;

bool ColorTableItem::operator<(const QTreeWidgetItem &other) const
{
  QString txt = text(0);
  QString other_txt = other.text(0);
  bool bRet = false;
  if (SortType == ColorTableItem::ST_VALUE)
  {
    bRet = (txt.split(" ", QString::SkipEmptyParts).first().toInt() >
        other_txt.split(" ", QString::SkipEmptyParts).first().toInt());
  }
  else
  {
//    if (txt.trimmed().contains(" "))
//      txt = txt.split(" ", QString::SkipEmptyParts).at(1);
//    if (other_txt.trimmed().contains(" "))
//      other_txt = other_txt.split(" ", QString::SkipEmptyParts).at(1);
    if (txt.toLower() != other_txt.toLower())
    {
      txt = txt.toLower();
      other_txt = other_txt.toLower();
    }
    bRet = (txt > other_txt);
  }
  if (!SortAscending)
    bRet = !bRet;
  return bRet;
}

PanelVolume::PanelVolume(QWidget *parent) :
  PanelLayer("MRI", parent),
  ui(new Ui::PanelVolume),
  m_curCTAB( NULL ),
  m_bShowExistingLabelsOnly(false)
{
  ui->setupUi(this);
  ui->treeWidgetColorTable->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(ui->treeWidgetColorTable, SIGNAL(customContextMenuRequested(QPoint)), SLOT(OnCustomContextMenu(QPoint)));
  ui->labelBrushValueWarning->hide();

  MainWindow* mainwnd = MainWindow::GetMainWindow();
  if ( !mainwnd )
  {
    return;
  }

  ui->toolbar->insertAction(ui->actionMoveLayerUp, mainwnd->ui->actionNewVolume);
  ui->toolbar->insertAction(ui->actionMoveLayerUp, mainwnd->ui->actionLoadVolume);
  ui->toolbar->insertAction(ui->actionMoveLayerUp, mainwnd->ui->actionCloseVolume);
  ui->toolbar->insertAction(ui->actionMoveLayerUp, mainwnd->ui->actionSaveVolume);
  ui->toolbar->insertSeparator(ui->actionMoveLayerUp);

  m_luts = mainwnd->GetLUTData();

  m_widgetlistGrayScale << ui->checkBoxClearBackground
                        << ui->labelWindow
                        << ui->labelLevel
                        << ui->lineEditWindow
                        << ui->lineEditLevel
                        << ui->sliderWindow
                        << ui->sliderLevel
                        << ui->checkBoxPercentile;

  m_widgetlistHeatScale << ui->sliderMid
                        << ui->sliderOffset
                        << ui->lineEditMid
                        << ui->lineEditOffset
                        << ui->checkBoxClearHigher
                        << ui->checkBoxTruncate
                        << ui->checkBoxSetMidToMin
                        << ui->checkBoxInvert
                        << ui->labelMid
                        << ui->labelOffset;

  m_widgetlistGenericColorMap << ui->lineEditMin
                              << ui->lineEditMax
                              << ui->sliderMin
                              << ui->sliderMax
                              << ui->labelMin
                              << ui->labelMax
                              << ui->pushButtonResetWindowLevel;

  m_widgetlistLUT << ui->treeWidgetColorTable
                  << ui->labelLookUpTable
                  << ui->comboBoxLookUpTable
                  << ui->colorLabelBrushValue
                  << ui->checkBoxShowExistingLabels
                  << ui->checkBoxSelectAllLabels;

  m_widgetlistDirectionCode << ui->comboBoxDirectionCode
                            << ui->labelDirectionCode;

  m_widgetlistFrame << ui->sliderFrame
                    << ui->spinBoxFrame
                    << ui->labelFrame
                    << ui->checkBoxAutoAdjustFrameLevel;
  //       << ui->checkBoxRememberFrame
  //       << ui->labelRememberFrame
  //       << ui->labelCorrelationSurface
  //       << ui->comboBoxCorrelationSurface;
  ui->labelRememberFrame->hide();
  ui->checkBoxRememberFrame->hide();
  ui->labelCorrelationSurface->hide();
  ui->comboBoxCorrelationSurface->hide();

  m_widgetlistVector << ui->labelInversion
                     << ui->comboBoxInversion
                     << ui->labelRenderObject
                     << ui->comboBoxRenderObject
                     << ui->checkBoxNormalizeVectors
                     << ui->lineEditVectorScale
                     << ui->labelVectorScale
                     << ui->labelVectorLineWidth
                     << ui->lineEditVectorLineWidth
                     << ui->labelVectorSkip
                     << ui->spinBoxVectorSkip;
  //    << ui->labelMask
  //    << ui->comboBoxMask;

  m_widgetlistContour << ui->sliderContourThresholdLow
                      << ui->sliderContourThresholdHigh
                      << ui->lineEditContourThresholdLow
                      << ui->lineEditContourThresholdHigh
                      << ui->checkBoxUseColorMap
                      << ui->checkBoxContourExtractAll
                      << ui->colorPickerContour
                      << ui->labelContourThresholdHigh
                      << ui->labelContourThresholdLow
                      << ui->labelContourColor
                      << ui->sliderContourSmoothIteration
                      << ui->lineEditContourSmoothIteration
                      << ui->labelSmoothIteration
                      << ui->pushButtonContourSave
                      << ui->labelContourLabelRange
                      << ui->lineEditContourLabelRange
                      << ui->checkBoxShowLabelContour
                      << ui->checkBoxUpsampleContour;

  m_widgetlistContourNormal << ui->sliderContourThresholdLow
                            << ui->sliderContourThresholdHigh
                            << ui->lineEditContourThresholdLow
                            << ui->lineEditContourThresholdHigh
                            << ui->checkBoxUseColorMap
                            << ui->checkBoxContourExtractAll
                            << ui->colorPickerContour
                            << ui->labelContourThresholdHigh
                            << ui->labelContourThresholdLow
                            << ui->labelContourColor
                            << ui->pushButtonContourSave;

  m_widgetlistContourLabel << ui->labelContourLabelRange
                           << ui->lineEditContourLabelRange;

  m_widgetlistEditable << ui->labelBrushValue
                       << ui->lineEditBrushValue;

  m_widgetlistNormalDisplay << ui->labelOpacity
                            << ui->sliderOpacity
                            << ui->doubleSpinBoxOpacity
                            << ui->checkBoxSmooth
                            << ui->checkBoxUpsample
                            << ui->labelColorMap
                            << ui->comboBoxColorMap;

  m_widgetlistVolumeTrack << ui->treeWidgetColorTable << m_widgetlistFrame
                          << ui->labelSmoothIteration << ui->sliderContourSmoothIteration
                          << ui->lineEditContourSmoothIteration;

  m_widgetlistVolumeTrackSpecs
      << ui->labelTrackVolumeThreshold
      << ui->sliderTrackVolumeThresholdLow
      << ui->lineEditTrackVolumeThresholdLow;

  QList<QWidget*> combo;
  combo << m_widgetlistGrayScale << m_widgetlistHeatScale
        << m_widgetlistGenericColorMap << m_widgetlistLUT
        << m_widgetlistDirectionCode << m_widgetlistVector
        << m_widgetlistContour << m_widgetlistEditable
        << ui->checkBoxSmooth << ui->checkBoxUpsample
        << ui->labelColorMap << ui->comboBoxColorMap
        << ui->checkBoxShowContour << ui->checkBoxShowOutline;

  combo = combo.toSet().toList();
  foreach (QWidget* w, m_widgetlistVolumeTrack)
  {
    int n = combo.indexOf(w);
    if (n >= 0)
      combo.removeAt(n);
  }
  m_widgetlistNonVolumeTrack = combo;

  LayerCollection* lc = mainwnd->GetLayerCollection("MRI");
  connect( ui->actionLockLayer, SIGNAL(toggled(bool)), this, SLOT(OnLockLayer(bool)) );
  connect( ui->actionMoveLayerUp, SIGNAL(triggered()), lc, SLOT(MoveLayerUp()));
  connect( ui->actionMoveLayerDown, SIGNAL(triggered()), lc, SLOT(MoveLayerDown()));

  connect( mainwnd, SIGNAL(MainViewChanged(int)), this, SLOT(UpdateWidgets()), Qt::QueuedConnection);
}

PanelVolume::~PanelVolume()
{
  delete ui;
}

void PanelVolume::ConnectLayer( Layer* layer_in )
{
  PanelLayer::ConnectLayer( layer_in );
  LayerMRI* layer = qobject_cast<LayerMRI*>(layer_in);
  if ( !layer )
  {
    return;
  }

  ui->progressBarWorking->hide();
  m_curCTAB = NULL;
  LayerPropertyMRI* p = layer->GetProperty();
  connect( p, SIGNAL(PropertyChanged()), this, SLOT(UpdateWidgets()), Qt::UniqueConnection );
  connect( ui->doubleSpinBoxOpacity, SIGNAL(valueChanged(double)), p, SLOT(SetOpacity(double)) );
  connect( ui->checkBoxSmooth, SIGNAL(stateChanged(int)), p, SLOT(SetTextureSmoothing(int)) );
  connect( ui->checkBoxShowContour, SIGNAL(clicked(bool)), p, SLOT(SetShowAsContour(bool)) );
  connect( ui->checkBoxShowLabelContour, SIGNAL(clicked(bool)), p, SLOT(SetShowAsLabelContour(bool)) );
  connect( ui->sliderFrame, SIGNAL(valueChanged(int)), layer, SLOT(SetActiveFrame(int)) );
  connect( ui->spinBoxFrame, SIGNAL(valueChanged(int)), layer, SLOT(SetActiveFrame(int)) );
  connect( ui->checkBoxDisplayVector, SIGNAL(toggled(bool)), this, SLOT(OnCheckBoxSetDisplayVector(bool)) );
  connect( ui->checkBoxDisplayTensor, SIGNAL(toggled(bool)), this, SLOT(OnCheckBoxSetDisplayTensor(bool)) );
  connect( ui->checkBoxDisplayRGB, SIGNAL(toggled(bool)), this, SLOT(OnCheckBoxSetDisplayRGB(bool)) );
  connect( ui->checkBoxNormalizeVectors, SIGNAL(toggled(bool)), this, SLOT(OnCheckBoxSetNormalizeVector(bool)) );
  connect( ui->comboBoxRenderObject, SIGNAL(currentIndexChanged(int)), p, SLOT(SetVectorRepresentation(int)) );
  connect( ui->comboBoxInversion, SIGNAL(currentIndexChanged(int)), p, SLOT(SetVectorInversion(int)) );
  connect( ui->comboBoxProjectionMapType, SIGNAL(currentIndexChanged(int)), this, SLOT(OnComboProjectionMapType(int)) );
  if ( layer->IsTypeOf( "DTI" ) )
    connect( ui->comboBoxDirectionCode, SIGNAL(currentIndexChanged(int)),
             qobject_cast<LayerDTI*>(layer)->GetProperty(), SLOT(SetDirectionCode(int)) );
  connect( layer, SIGNAL(ActiveFrameChanged(int)), this, SLOT(UpdateWidgets()) );
  connect( layer, SIGNAL(ActiveFrameChanged(int)), this, SLOT(OnActiveFrameChanged(int)));
  connect( layer, SIGNAL(FillValueChanged(double)), this, SLOT(UpdateWidgets()));
  connect( layer, SIGNAL(LabelStatsReady()), this, SLOT(UpdateWidgets()));
  connect( layer, SIGNAL(LabelStatsReady()), this, SLOT(OnLineEditBrushValue()));
  connect( ui->checkBoxClearBackground, SIGNAL(toggled(bool)), p, SLOT(SetClearZero(bool)) );
  connect( ui->checkBoxClearHigher, SIGNAL(toggled(bool)), p, SLOT(SetHeatScaleClearHigh(bool)) );
  connect( ui->checkBoxSetMidToMin, SIGNAL(toggled(bool)), p, SLOT(SetHeatScaleAutoMid(bool)));
  connect( ui->checkBoxTruncate, SIGNAL(toggled(bool)), p, SLOT(SetHeatScaleTruncate(bool)) );
  connect( ui->checkBoxInvert, SIGNAL(toggled(bool)), p, SLOT(SetHeatScaleInvert(bool)) );
  connect( ui->checkBoxShowOutline, SIGNAL(toggled(bool)), p, SLOT(SetShowLabelOutline(bool)) );
  connect( ui->checkBoxContourExtractAll, SIGNAL(toggled(bool)), p, SLOT(SetContourExtractAllRegions(bool)) );
  connect( ui->checkBoxUseColorMap, SIGNAL(toggled(bool)), p, SLOT(SetContourUseImageColorMap(bool)) );
  connect( ui->checkBoxShowInfo, SIGNAL(toggled(bool)), p, SLOT(SetShowInfo(bool)) );
  connect( ui->colorPickerContour, SIGNAL(colorChanged(QColor)), p, SLOT(SetContourColor(QColor)));
  connect( ui->checkBoxUpsampleContour, SIGNAL(toggled(bool)), p, SLOT(SetContourUpsample(bool)));
  connect( ui->checkBoxRememberFrame, SIGNAL(toggled(bool)), p, SLOT(SetRememberFrameSettings(bool)));
  connect( ui->checkBoxAutoAdjustFrameLevel, SIGNAL(toggled(bool)), p, SLOT(SetAutoAdjustFrameLevel(bool)));
  connect( ui->lineEditProjectionMapRange, SIGNAL(returnPressed()), this, SLOT(OnLineEditProjectionMapRangeChanged()));
  connect( layer, SIGNAL(IsoSurfaceUpdating()), ui->progressBarWorking, SLOT(show()));
  connect( layer, SIGNAL(IsoSurfaceUpdated()), ui->progressBarWorking, SLOT(hide()));
  connect( ui->pushButtonResetWindowLevel, SIGNAL(clicked(bool)), SLOT(OnButtonResetWindowLevel()));
  connect( ui->spinBoxVectorSkip, SIGNAL(valueChanged(int)), p, SLOT(SetVectorSkip(int)));
}

void PanelVolume::DoIdle()
{
  // update action status
  BlockAllSignals( true );
  LayerMRI* layer = GetCurrentLayer<LayerMRI*>();
  ui->actionMoveLayerUp->setEnabled(layer && m_layerCollection
                                    && m_layerCollection->GetLayerIndex(layer) > 0);
  ui->actionMoveLayerDown->setEnabled(layer && m_layerCollection
                                      && m_layerCollection->GetLayerIndex(layer) < m_layerCollection->GetNumberOfLayers()-1);
  ui->actionLockLayer->setEnabled( layer );
  ui->actionLockLayer->setChecked( layer && layer->IsLocked() );
  ui->actionCopySetting->setEnabled(layer);

  QStringList strgs = qApp->clipboard()->text().split(",");
  bool bDataAvail = (!strgs.isEmpty() && strgs[0] == FS_VOLUME_SETTING_ID);
  ui->actionPasteSetting->setEnabled(layer && bDataAvail);
  ui->actionPasteSettingToAll->setEnabled(layer && bDataAvail);
  BlockAllSignals( false );
}

void PanelVolume::DoUpdateWidgets()
{
  BlockAllSignals( true );

  LayerMRI* layer = GetCurrentLayer<LayerMRI*>();
  for ( int i = 0; i < this->allWidgets.size(); i++ )
  {
    if ( allWidgets[i] != ui->toolbar && allWidgets[i]->parentWidget() != ui->toolbar &&
         allWidgets[i] != ui->toolbar2 && allWidgets[i]->parentWidget() != ui->toolbar2 )
    {
      allWidgets[i]->setEnabled(layer);
    }
  }
  int nColorMap = LayerPropertyMRI::NoColorMap;
  int nMode = MainWindow::GetMainWindow()->GetMode();
  ui->lineEditFileName->clear();
  if ( layer )
  {
    nColorMap = layer->GetProperty()->GetColorMap();
    bool bPercentile = layer->GetProperty()->GetUsePercentile();
    ui->checkBoxPercentile->setChecked(bPercentile);
    ui->checkBoxPercentile->setVisible(layer->GetProperty()->GetColorMap() != LayerPropertyMRI::LUT && layer->HasValidHistogram());
    ui->sliderOpacity->setValue( (int)( layer->GetProperty()->GetOpacity() * 100 ) );
    ChangeDoubleSpinBoxValue( ui->doubleSpinBoxOpacity, layer->GetProperty()->GetOpacity() );
    ui->checkBoxClearBackground->setChecked( layer->GetProperty()->GetClearZero() );
    if ( layer->IsTypeOf( "DTI" ) )
    {
      ui->lineEditFileName->setText(MyUtils::Win32PathProof(((LayerDTI*)layer)->GetVectorFileName()) );
    }
    else
    {
      ui->lineEditFileName->setText( MyUtils::Win32PathProof(layer->GetFileName()) );
    }
    ui->lineEditFileName->setCursorPosition( ui->lineEditFileName->text().size() );

    ui->checkBoxSmooth->setChecked( layer->GetProperty()->GetTextureSmoothing() );
    ui->checkBoxUpsample->setChecked( layer->GetProperty()->GetUpSampleMethod() != 0 );
    ui->checkBoxUpsample->setEnabled( layer->GetProperty()->GetColorMap() != LayerPropertyMRI::LUT );

    // color map settings
    ui->comboBoxColorMap->setCurrentIndex( layer->GetProperty()->GetColorMap() );
    ui->comboBoxLookUpTable->clear();
    for ( int i = 0; i < m_luts->GetCount(); i++ )
    {
      ui->comboBoxLookUpTable->addItem( m_luts->GetName( i ) );
    }
    if ( layer->GetEmbeddedColorTable() )
    {
      ui->comboBoxLookUpTable->addItem( "Embedded" );
    }
    ui->comboBoxLookUpTable->addItem( "Load lookup table..." );
    int nSel = m_luts->GetIndex( layer->GetProperty()->GetLUTCTAB() );
    ui->comboBoxLookUpTable->setCurrentIndex( nSel >= 0 ? nSel : m_luts->GetCount() );

    ChangeLineEditNumber( ui->lineEditBrushValue, layer->GetFillValue() );
    ui->lineEditBrushValue->setEnabled(nMode != RenderView::IM_ReconEdit);
    double dwindow = layer->GetProperty()->GetWindow();
    double dlevel  = layer->GetProperty()->GetLevel();
    ChangeLineEditNumber( ui->lineEditWindow, dwindow );
    ChangeLineEditNumber( ui->lineEditLevel, dlevel );
    double dMinTh = dlevel - dwindow/2;
    double dMaxTh = dlevel + dwindow/2;
    double dminvalue = layer->GetProperty()->GetMinValue();
    double dmaxvalue = layer->GetProperty()->GetMaxValue();
    double range_min = dminvalue - (dmaxvalue-dminvalue)/4;
    double range_max = dmaxvalue + (dmaxvalue-dminvalue)/4;
    if ( nColorMap == LayerPropertyMRI::Heat )
    {
      dMinTh = layer->GetProperty()->GetHeatScaleMinThreshold();
      dMaxTh = layer->GetProperty()->GetHeatScaleMaxThreshold();
      range_min = dminvalue;
      range_max = dmaxvalue;
    }
    else if ( nColorMap != LayerPropertyMRI::Grayscale && nColorMap != LayerPropertyMRI::LUT)
    {
      dMinTh = layer->GetProperty()->GetMinGenericThreshold();
      dMaxTh = layer->GetProperty()->GetMaxGenericThreshold();
    }
    double* windowrange = layer->GetProperty()->GetWindowRange();
    double* levelrange = layer->GetProperty()->GetLevelRange();
    ui->sliderWindow->setValue( (int)( ( dwindow - windowrange[0] ) / ( windowrange[1] - windowrange[0] ) * 100 ) );
    ui->sliderLevel->setValue( (int)( ( dlevel - levelrange[0] ) / ( levelrange[1] - levelrange[0] ) * 100 ) );

    double dHeatMidTh = layer->GetProperty()->GetHeatScaleMidThreshold();
    double dHeatOffset = layer->GetProperty()->GetHeatScaleOffset();
    if (bPercentile)
    {
      dMaxTh = (layer->GetHistoPercentileFromValue(dMaxTh)*100);
      dMinTh = (layer->GetHistoPercentileFromValue(dMinTh)*100);
      dHeatMidTh = (layer->GetHistoPercentileFromValue(dHeatMidTh)*100);
      range_min = 0;
      range_max = 100;
    }
    ChangeLineEditNumber( ui->lineEditMax, dMaxTh );
    ChangeLineEditNumber( ui->lineEditMin, dMinTh );
    ui->sliderMin->setValue( (int)( ( dMinTh - range_min ) / ( range_max - range_min ) * 100 ) );
    ui->sliderMax->setValue( (int)( ( dMaxTh - range_min ) / ( range_max - range_min ) * 100 ) );

    ui->sliderMid->setValue( (int)( ( dHeatMidTh - range_min ) /
                                    ( range_max - range_min ) * 100 ) );
    ui->sliderOffset->setValue( (int)( ( dHeatOffset + dmaxvalue ) /
                                       ( dmaxvalue + dmaxvalue ) * 100 ) );
    ChangeLineEditNumber( ui->lineEditMid, dHeatMidTh );
    ChangeLineEditNumber( ui->lineEditOffset, dHeatOffset );
    ui->checkBoxClearHigher->setChecked( layer->GetProperty()->GetHeatScaleClearHigh() );
    ui->checkBoxTruncate->setChecked( layer->GetProperty()->GetHeatScaleTruncate() );
    ui->checkBoxInvert->setChecked( layer->GetProperty()->GetHeatScaleInvert() );
    ui->checkBoxSetMidToMin->setChecked( layer->GetProperty()->GetHeatScaleAutoMid());

    ui->comboBoxColorMap->clear();
    ui->comboBoxColorMap->addItem( "Grayscale", LayerPropertyMRI::Grayscale );
    if ( layer->IsTypeOf( "DTI" ) )
    {
      ui->comboBoxColorMap->addItem( "Direction-coded", LayerPropertyMRI::DirectionCoded );
      ui->comboBoxDirectionCode->setCurrentIndex( ((LayerDTI*)layer)->GetProperty()->GetDirectionCode() );
    }
    else
    {
      ui->comboBoxColorMap->addItem( "Lookup Table", LayerPropertyMRI::LUT );
    }
    ui->comboBoxColorMap->addItem( "Heat", LayerPropertyMRI::Heat );
    ui->comboBoxColorMap->addItem( "Jet", LayerPropertyMRI::Jet );
    ui->comboBoxColorMap->addItem( "GE Color", LayerPropertyMRI::GEColor );
    ui->comboBoxColorMap->addItem( "NIH", LayerPropertyMRI::NIH );
    ui->comboBoxColorMap->addItem( "PET", LayerPropertyMRI::PET );
    for ( int i = 0; i < ui->comboBoxColorMap->count(); i++ )
    {
      if ( ui->comboBoxColorMap->itemData( i ).toInt() == nColorMap )
      {
        ui->comboBoxColorMap->setCurrentIndex( i );
        break;
      }
    }

    int nFrames = layer->GetNumberOfFrames();
    if ( nFrames > 1 )
    {
      ui->sliderFrame->setRange( 0, nFrames-1 );
      ui->spinBoxFrame->setRange( 0, nFrames-1 );
      ui->checkBoxRememberFrame->setChecked(layer->GetProperty()->GetRememberFrameSettings());
      ui->checkBoxAutoAdjustFrameLevel->setChecked(layer->GetProperty()->GetAutoAdjustFrameLevel());
    }
    ui->sliderFrame->setValue( layer->GetActiveFrame() );
    ChangeSpinBoxValue( ui->spinBoxFrame, layer->GetActiveFrame() );

    ui->checkBoxShowContour->setChecked( layer->GetProperty()->GetShowAsContour() );
    ui->sliderContourThresholdLow->setValue( (int)( ( layer->GetProperty()->GetContourMinThreshold() - dminvalue ) / ( dmaxvalue - dminvalue ) * 100 ) );
    ui->sliderContourThresholdHigh->setValue( (int)( ( layer->GetProperty()->GetContourMaxThreshold() - dminvalue ) / ( dmaxvalue - dminvalue ) * 100 ) );
    ChangeLineEditNumber( ui->lineEditContourThresholdLow, layer->GetProperty()->GetContourMinThreshold() );
    ChangeLineEditNumber( ui->lineEditContourThresholdHigh, layer->GetProperty()->GetContourMaxThreshold() );
    ui->checkBoxUseColorMap->setChecked( layer->GetProperty()->GetContourUseImageColorMap() );
    ui->checkBoxUpsampleContour->setChecked( layer->GetProperty()->GetContourUpsample());
    ui->checkBoxContourExtractAll->setChecked( layer->GetProperty()->GetContourExtractAllRegions() );
    ui->sliderContourSmoothIteration->setValue( layer->GetProperty()->GetContourSmoothIterations() );
    ChangeLineEditNumber( ui->lineEditContourSmoothIteration, layer->GetProperty()->GetContourSmoothIterations() );
    ui->lineEditContourLabelRange->setText(layer->GetProperty()->GetLabelContourRange().trimmed());

    ui->colorPickerContour->setEnabled( !layer->GetProperty()->GetContourUseImageColorMap() );
    double rgb[3];
    layer->GetProperty()->GetContourColor( rgb );
    ui->colorPickerContour->setCurrentColor( QColor( (int)(rgb[0]*255), (int)(rgb[1]*255), (int)(rgb[2]*255) ) );

    ui->comboBoxRenderObject->clear();
    if ( layer->GetProperty()->GetDisplayVector() )
    {
      ui->comboBoxRenderObject->addItem( "Line" );
      ui->comboBoxRenderObject->addItem( "Line With Direction" );
      ui->comboBoxRenderObject->addItem( "3D Bar (slow!)" );
      ui->comboBoxRenderObject->setCurrentIndex( layer->GetProperty()->GetVectorRepresentation() );
      ui->comboBoxInversion->setCurrentIndex( layer->GetProperty()->GetVectorInversion() );
    }
    else if ( layer->GetProperty()->GetDisplayTensor() )
    {
      ui->comboBoxRenderObject->addItem( "Boxoid" );
      ui->comboBoxRenderObject->addItem( "Ellipsoid (Very slow!)" );
      ui->comboBoxRenderObject->setCurrentIndex( layer->GetProperty()->GetTensorRepresentation() );
      ui->comboBoxInversion->setCurrentIndex( layer->GetProperty()->GetTensorInversion() );
    }
    ui->checkBoxNormalizeVectors->setChecked(layer->GetProperty()->GetNormalizeVector());
    ChangeLineEditNumber( ui->lineEditVectorScale, layer->GetProperty()->GetVectorDisplayScale());
    ChangeLineEditNumber( ui->lineEditVectorLineWidth, layer->GetProperty()->GetVectorLineWidth());

    ui->checkBoxShowInfo->setChecked( layer->GetProperty()->GetShowInfo() );

    ui->checkBoxShowOutline->setChecked( layer->GetProperty()->GetShowLabelOutline() );
    ui->widgetProjectionMapType->setVisible(!layer->IsTypeOf("DTI") );
    ui->widgetProjectionMapSettings->setVisible(ui->widgetProjectionMapType->isVisible() && layer->GetProperty()->GetShowProjectionMap());
    ui->comboBoxProjectionMapType->setCurrentIndex(layer->GetProperty()->GetProjectionMapType());
    int nRange[2];
    int nPlane = MainWindow::GetMainWindow()->GetMainViewId();
    if (nPlane > 2)
      nPlane = 2;
    layer->GetProperty()->GetProjectionMapRange(nPlane, nRange);
    if (nRange[1] < 0)
    {
      int* dim = layer->GetImageData()->GetDimensions();
      nRange[1] = dim[nPlane]-1;
    }
    ui->lineEditProjectionMapRange->setText(QString("%1, %2").arg(nRange[0]).arg(nRange[1]));
    ui->checkBoxShowOutline->setVisible(ui->comboBoxProjectionMapType->currentIndex() == 0 && !layer->IsTypeOf("DTI"));

    //    ui->m_choiceUpSampleMethod->SetSelection( layer->GetProperty()->GetUpSampleMethod() );
    ui->checkBoxShowExistingLabels->setEnabled(!layer->GetAvailableLabels().isEmpty());

    // mask layer setting
    ui->comboBoxMask->clear();
    ui->comboBoxMask->addItem("None");
    QList<Layer*> layers = MainWindow::GetMainWindow()->GetLayers("MRI");
    int n = 0;
    for (int i = 0; i < layers.size(); i++)
    {
      if (layer != layers[i])
      {
        ui->comboBoxMask->addItem( layers[i]->GetName(),  QVariant::fromValue((QObject*)layers[i]) );
        if (layer->GetMaskLayer() == layers[i])
          n = ui->comboBoxMask->count()-1;
      }
    }
    ui->comboBoxMask->setCurrentIndex(n);

    ui->labelMaskThreshold->setVisible(n > 0);
    ui->lineEditMaskThreshold->setVisible(n > 0);
    ChangeLineEditNumber(ui->lineEditMaskThreshold, layer->GetMaskThreshold());

    // correlation surface setting
    if (layer->GetNumberOfFrames() > 1)
    {
      ui->comboBoxCorrelationSurface->clear();
      ui->comboBoxCorrelationSurface->addItem("None");
      QList<Layer*> surfs = MainWindow::GetMainWindow()->GetLayers("Surface");
      n = 0;
      for (int i = 0; i < surfs.size(); i++)
      {
        LayerSurface* surf = ((LayerSurface*)surfs[i]);
        for (int j = 0; j < surf->GetNumberOfOverlays(); j++)
        {
          SurfaceOverlay* overlay = surf->GetOverlay(j);
          if (overlay->GetNumberOfFrames() == layer->GetNumberOfFrames())
          {
            ui->comboBoxCorrelationSurface->addItem(surf->GetName(), QVariant::fromValue((QObject*)surf));
            if (surf == layer->GetCorrelationSurface())
              n = ui->comboBoxCorrelationSurface->count()-1;
            break;
          }
        }
      }
      ui->comboBoxCorrelationSurface->setCurrentIndex(n);
    }

    ui->spinBoxVectorSkip->setValue(layer->GetProperty()->GetVectorSkip());
  }

  bool bNormalDisplay = (layer && !layer->GetProperty()->GetDisplayVector()
                         && !layer->GetProperty()->GetDisplayTensor() && !layer->GetProperty()->GetDisplayRGB());

  if (layer && layer->IsTypeOf("VolumeTrack"))
  {
    ShowWidgets(m_widgetlistNonVolumeTrack, false);
    ShowWidgets(m_widgetlistVolumeTrack, true);
    if (m_curCTAB != layer->GetEmbeddedColorTable())
      PopulateColorTable( layer->GetEmbeddedColorTable() );
  }
  else
  {
    ShowWidgets( m_widgetlistNormalDisplay, bNormalDisplay );
    ShowWidgets( m_widgetlistGrayScale, bNormalDisplay && nColorMap == LayerPropertyMRI::Grayscale );
    ShowWidgets( m_widgetlistHeatScale, bNormalDisplay && nColorMap == LayerPropertyMRI::Heat );
    ShowWidgets( m_widgetlistGenericColorMap, (bNormalDisplay && nColorMap != LayerPropertyMRI::LUT &&
        nColorMap != LayerPropertyMRI::DirectionCoded) ||
                 (layer && layer->IsTypeOf("DTI") && !layer->GetProperty()->GetDisplayVector() && !layer->GetProperty()->GetDisplayRGB()) );
    ShowWidgets( m_widgetlistLUT, bNormalDisplay && nColorMap == LayerPropertyMRI::LUT );
    ShowWidgets( m_widgetlistDirectionCode, bNormalDisplay && nColorMap == LayerPropertyMRI::DirectionCoded );
    ShowWidgets( m_widgetlistEditable, bNormalDisplay && layer->IsEditable() );
    ShowWidgets( m_widgetlistFrame, layer &&
                 !layer->IsTypeOf( "DTI" ) &&
                 layer->GetNumberOfFrames() > 1 && !layer->GetCorrelationSurface() );
    ui->labelCorrelationSurface->setVisible(layer && layer->GetNumberOfFrames() > 1 && ui->comboBoxCorrelationSurface->count() > 1);
    ui->comboBoxCorrelationSurface->setVisible(ui->labelCorrelationSurface->isVisible());

    ui->sliderFrame->setEnabled( layer &&
                                 !layer->GetProperty()->GetDisplayVector() &&
                                 !layer->GetProperty()->GetDisplayTensor() &&
                                 !layer->GetProperty()->GetDisplayRGB());
    ui->spinBoxFrame->setEnabled( layer &&
                                  !layer->GetProperty()->GetDisplayVector() &&
                                  !layer->GetProperty()->GetDisplayTensor() );
    ui->checkBoxDisplayVector->setVisible( layer && ( layer->IsTypeOf( "DTI" ) || layer->GetNumberOfFrames() == 3 ) );
    ui->checkBoxDisplayVector->setChecked( layer && layer->GetProperty()->GetDisplayVector() );
    ui->checkBoxDisplayTensor->setVisible( layer && layer->GetNumberOfFrames() == 9 );
    ui->checkBoxDisplayTensor->setChecked( layer && layer->GetProperty()->GetDisplayTensor() );
    ui->checkBoxDisplayRGB->setVisible(layer && layer->GetNumberOfFrames() == 3);
    ui->checkBoxDisplayRGB->setChecked(layer && layer->GetProperty()->GetDisplayRGB());
    ShowWidgets( m_widgetlistVector, ui->checkBoxDisplayVector->isChecked() || ui->checkBoxDisplayTensor->isChecked() );
    ShowWidgets( m_widgetlistContour, ui->checkBoxShowContour->isChecked() && !layer->GetProperty()->GetDisplayRGB() );

    ui->checkBoxShowContour->setVisible( bNormalDisplay && !layer->GetProperty()->GetShowProjectionMap() );
    ui->checkBoxShowContour->setEnabled( nColorMap != LayerPropertyMRI::LUT || ui->checkBoxShowExistingLabels->isEnabled());
    if (layer && ui->checkBoxShowContour->isChecked())
    {
      ui->checkBoxShowLabelContour->setChecked(layer->GetProperty()->GetShowAsLabelContour());
      ShowWidgets( m_widgetlistContourNormal, !layer->GetProperty()->GetShowAsLabelContour());
      ShowWidgets( m_widgetlistContourLabel, false); //layer->GetProperty()->GetShowAsLabelContour());
    }

    //  ShowWidgets( m_widgetlistContour, false );
    //  m_checkContour->Show( false /*nColorMap == LayerPropertyMRI::LUT*/ );

    ui->checkBoxShowExistingLabels->setChecked(m_bShowExistingLabelsOnly);
    if ( layer && layer->GetProperty()->GetColorMap() == LayerPropertyMRI::LUT )
    {
      if ( m_curCTAB != layer->GetProperty()->GetLUTCTAB()) // || m_bShowExistingLabelsOnly != ui->checkBoxShowExistingLabels->isChecked())
      {
        PopulateColorTable( layer->GetProperty()->GetLUTCTAB() );
      }

      for ( int i = 0; i < ui->treeWidgetColorTable->topLevelItemCount(); i++ )
      {
        QTreeWidgetItem* item = ui->treeWidgetColorTable->topLevelItem( i );
        QStringList strglist = item->text(0).split( " " );
        bool bOK;
        double dvalue = strglist[0].trimmed().toDouble( &bOK );
        if ( bOK && dvalue == layer->GetFillValue() )
        {
          ui->treeWidgetColorTable->setCurrentItem( item );
          break;
        }
      }
      UpdateColorLabel();
    }
  }
  if (layer && nColorMap == LayerPropertyMRI::Heat)
  {
    bool bAutoMid = layer->GetProperty()->GetHeatScaleAutoMid();
    ui->labelMid->setEnabled(!bAutoMid);
    ui->sliderMid->setEnabled(!bAutoMid);
    ui->lineEditMid->setEnabled(!bAutoMid);
  }

  ui->labelBrushValueWarning->hide();
  if (layer)
  {
    if (nColorMap == LayerPropertyMRI::LUT && !layer->GetProperty()->IsValueInColorTable(layer->GetFillValue()))
    {
      ui->colorLabelBrushValue->hide();
      ui->labelBrushValueWarning->show();
    }
  }

  UpdateTrackVolumeThreshold();

  ui->checkBoxUpsampleContour->hide();
  ui->checkBoxUpsample->hide();

  BlockAllSignals( false );
}

void PanelVolume::OnColorTableCurrentItemChanged( QTreeWidgetItem* item )
{
  if ( item )
  {
    double val = item->data(0, Qt::UserRole+1).toDouble();
    LayerMRI* layer = GetCurrentLayer<LayerMRI*>();
    if ( layer->IsTypeOf("VolumeTrack") )
    {
      UpdateTrackVolumeThreshold();
    }
    else
    {
      layer->SetFillValue( val );
    }
    ChangeLineEditNumber( ui->lineEditBrushValue, val );
    MainWindow::GetMainWindow()->GetBrushProperty()->SetFillValue(val);
    UpdateColorLabel();
  }
}

void PanelVolume::OnColorTableItemDoubleClicked(QTreeWidgetItem *item_in)
{
  QTreeWidgetItem* item = item_in;
  if (!item)
    item = ui->treeWidgetColorTable->currentItem();
  if (item)
  {
    QStringList strglist = item->text( 0 ).split(" ", QString::SkipEmptyParts);
    double val = strglist[0].toDouble();
    LayerMRI* layer = GetCurrentLayer<LayerMRI*>();
    if ( layer )
    {
      double pos[3];
      if (layer->GetLayerLabelCenter(val, pos))
      {
        MainWindow::GetMainWindow()->SetSlicePosition(pos);
        MainWindow::GetMainWindow()->CenterAtWorldPosition(pos);
      }
      else
      {
        qDebug() << tr("Label %1 does not exist").arg(item->text(0));
      }
    }
  }
}

void PanelVolume::OnColorTableSortingChanged()
{
  if (sender())
  {
//    if (sender()->property("sort_by").toInt() == ColorTableItem::SortType)
//      ColorTableItem::SortAscending = !ColorTableItem::SortAscending;
//    else
      ColorTableItem::SortType = sender()->property("sort_by").toInt();
    BlockAllSignals(true);
    COLOR_TABLE* t = m_curCTAB;
    m_curCTAB = NULL;
    PopulateColorTable(t);
    BlockAllSignals(false);
  }
}

void PanelVolume::UpdateColorLabel()
{
  QTreeWidgetItem* item = ui->treeWidgetColorTable->currentItem();
  LayerMRI* layer = GetCurrentLayer<LayerMRI*>();
  if (!layer || layer->GetProperty()->GetColorMap() != LayerPropertyMRI::LUT)
    return;

  if ( item )
  {
    QColor color = item->data( 0, Qt::UserRole ).value<QColor>();
    if ( color.isValid() )
    {
      QPixmap pix( 30, 20 );
      pix.fill( color );
      ui->colorLabelBrushValue->setPixmap( pix );
      ui->colorLabelBrushValue->show();
      ui->labelBrushValueWarning->hide();
    }
  }
  else
  {
    ui->colorLabelBrushValue->hide();
    bool bOK;
    ui->lineEditBrushValue->text().trimmed().toInt(&bOK);
    ui->labelBrushValueWarning->setVisible(bOK);
  }
}

void PanelVolume::UpdateTrackVolumeThreshold()
{
  LayerVolumeTrack* layer = GetCurrentLayer<LayerVolumeTrack*>();
  QTreeWidgetItem* item = ui->treeWidgetColorTable->currentItem();
  if ( item && layer )
  {
    int nLabel = item->text(0).split(" ").at(0).toInt();
    ui->sliderTrackVolumeThresholdLow->blockSignals(true);
    ui->lineEditTrackVolumeThresholdLow->blockSignals(true);
    double fMin = layer->GetProperty()->GetMinValue();
    double fMax = layer->GetProperty()->GetMaxValue()/4;
    ui->sliderTrackVolumeThresholdLow->setValue( (int)( ( layer->GetThreshold(nLabel) - fMin ) / ( fMax - fMin ) * 100 ) );
    ChangeLineEditNumber( ui->lineEditTrackVolumeThresholdLow, layer->GetThreshold(nLabel) );
    ui->sliderTrackVolumeThresholdLow->blockSignals(false);
    ui->lineEditTrackVolumeThresholdLow->blockSignals(false);
    layer->Highlight(nLabel);
  }
  ShowWidgets(m_widgetlistVolumeTrackSpecs, layer);
  EnableWidgets(this->m_widgetlistVolumeTrackSpecs, item);
}

void PanelVolume::PopulateColorTable( COLOR_TABLE* ct )
{
  if ( ct && ct != m_curCTAB )
  {
    m_curCTAB = ct;
    ui->treeWidgetColorTable->clear();
    int nTotalCount = 0;
    CTABgetNumberOfTotalEntries( ct, &nTotalCount );
    int nValid = 0;
    char name[1000];
    int nSel = -1;
    int nValue = 0;
    bool bOK;
    nValue = ui->lineEditBrushValue->text().toInt( &bOK );
    LayerMRI* layer = GetCurrentLayer<LayerMRI*>();
    if ( !bOK && layer )
      nValue = (int)layer->GetFillValue();

    QList<int> labels;
    QList<int> selectedLabels;
    if (layer)
    {
      labels = layer->GetAvailableLabels();
      selectedLabels = layer->GetProperty()->GetSelectedLabels();
    }
    int nValidCount = 0;
    bool bHasSelected = false, bHasUnselected = false;
    for ( int i = 0; i < nTotalCount; i++ )
    {
      CTABisEntryValid( ct, i, &nValid );
      if ( nValid )
      {
        CTABcopyName( ct, i, name, 1000 );
        ColorTableItem* item = new ColorTableItem( ui->treeWidgetColorTable );
        if (ColorTableItem::SortType == ColorTableItem::ST_VALUE)
          item->setText( 0, QString("%1 %2").arg(i).arg(name) );
        else
          item->setText(0, QString("%1 (%2)").arg(name).arg(i));
        item->setToolTip( 0, name );
        int nr, ng, nb;
        CTABrgbAtIndexi( ct, i, &nr, &ng, &nb );
        QColor color( nr, ng, nb );
        QPixmap pix(13, 13);
        pix.fill( color );
        item->setIcon(0, QIcon(pix) );
        item->setData(0, Qt::UserRole, color );
        item->setData(0, Qt::UserRole+1, i);
        item->setCheckState(0,  selectedLabels.contains(i)?Qt::Checked:Qt::Unchecked);
        if (i > 0)
        {
          if (item->checkState(0) == Qt::Checked)
            bHasSelected = true;
          else
            bHasUnselected = true;
        }
        if (m_bShowExistingLabelsOnly && !labels.isEmpty())
        {
          item->setHidden(!labels.contains(i));
        }
        if ( i == nValue )
        {
          nSel = nValidCount;
        }
        nValidCount++;
      }
    }
    if ( nSel >= 0 )
    {
      ui->treeWidgetColorTable->setCurrentItem( ui->treeWidgetColorTable->topLevelItem( nSel ) );
    }
    if (bHasSelected && !bHasUnselected)
      ui->checkBoxSelectAllLabels->setCheckState(Qt::Checked);
    else if (bHasSelected)
      ui->checkBoxSelectAllLabels->setCheckState(Qt::PartiallyChecked);
    else
      ui->checkBoxSelectAllLabels->setCheckState(Qt::Unchecked);
  }
}

void PanelVolume::OnLineEditBrushValue( const QString& strg )
{
  QString text = strg.trimmed();
  bool bOK;
  int nVal = text.toInt( &bOK );
  LayerMRI* layer = GetCurrentLayer<LayerMRI*>();
  if (!layer)
    return;

  QList<int> labels = layer->GetAvailableLabels();
  if ( text.isEmpty() )
  {
    for ( int i = 0; i < ui->treeWidgetColorTable->topLevelItemCount(); i++ )
    {
      QTreeWidgetItem* item = ui->treeWidgetColorTable->topLevelItem( i );
      if (m_bShowExistingLabelsOnly)
      {
        int n = item->data(0, Qt::UserRole+1).toInt();
        item->setHidden(!labels.contains(n));
      }
      else
        item->setHidden( false );
    }
    ui->labelBrushValueWarning->hide();
  }
  else if ( bOK )
  {
    MainWindow::GetMainWindow()->GetBrushProperty()->SetFillValue(nVal);
    bool bFound = false;
    for ( int i = 0; i < ui->treeWidgetColorTable->topLevelItemCount(); i++ )
    {
      QTreeWidgetItem* item = ui->treeWidgetColorTable->topLevelItem( i );
      int n = item->data(0, Qt::UserRole+1).toInt();
      if (m_bShowExistingLabelsOnly)
        item->setHidden(!labels.contains(n));
      else
        item->setHidden( false );
      if ( n == layer->GetFillValue() )
      {
        ui->treeWidgetColorTable->setCurrentItem( item );
        bFound = true;
      }
    }
    if( !bFound )
    {
      ui->treeWidgetColorTable->setCurrentItem( NULL );
    }
    UpdateColorLabel();
  }
  else
  {
    ui->labelBrushValueWarning->hide();
    QStringList keywords = text.split(" ", QString::SkipEmptyParts);
    for ( int i = 0; i < ui->treeWidgetColorTable->topLevelItemCount(); i++ )
    {
      QTreeWidgetItem* item = ui->treeWidgetColorTable->topLevelItem( i );
      bool bFound = true;
      QString item_text = item->text(0);
      foreach (QString key, keywords)
      {
        if (!item_text.contains(key, Qt::CaseInsensitive))
        {
          bFound = false;
          break;
        }
      }
      if (bFound)
      {
        if (m_bShowExistingLabelsOnly)
        {
          int n = item->data(0, Qt::UserRole+1).toInt();
          item->setHidden(!labels.contains(n));
        }
        else
          item->setHidden( false );
      }
      else
      {
        item->setHidden( true );
      }
    }
  }
}

void PanelVolume::OnComboColorMap( int nSel )
{
  /*
  LayerMRI* layer = GetCurrentLayer<LayerMRI*>();
  if ( layer && nSel >= 0 )
  {
    nSel = ui->comboBoxColorMap->itemData(nSel).toInt();
    layer->GetProperty()->SetColorMap( nSel );
  }
  */
  QList<LayerMRI*> layers = GetSelectedLayers<LayerMRI*>();
  foreach (LayerMRI* layer, layers)
  {
    if (nSel >= 0)
    {
      nSel = ui->comboBoxColorMap->itemData(nSel).toInt();
      layer->GetProperty()->SetColorMap( nSel );
    }
  }
}

void PanelVolume::OnComboLookupTable( int nSel )
{
  QList<LayerMRI*> layers = GetSelectedLayers<LayerMRI*>();
  foreach (LayerMRI* layer, layers)
  {
    if ( nSel == ui->comboBoxLookUpTable->count()-1 )
    {
      MainWindow::GetMainWindow()->LoadLUT();
    }
    else
    {
      if ( nSel < m_luts->GetCount() )
      {
        COLOR_TABLE* ct = m_luts->GetColorTable( nSel );
        layer->GetProperty()->SetLUTCTAB( ct );
      }
      else
      {
        layer->GetProperty()->SetLUTCTAB( layer->GetEmbeddedColorTable() );
      }
    }
  }
}

void PanelVolume::OnCheckShowContour(bool bShow)
{
  ShowWidgets( m_widgetlistContour, bShow );
}

void PanelVolume::OnCheckShowLabelContour(bool bShow)
{
  ShowWidgets( m_widgetlistContourNormal, !bShow);
//  ShowWidgets( m_widgetlistContourLabel, bShow);
}

void PanelVolume::OnSliderOpacity( int nVal )
{
  QList<LayerMRI*> layers = GetSelectedLayers<LayerMRI*>();
  foreach (LayerMRI* layer, layers)
  {
    layer->GetProperty()->SetOpacity( nVal/100.0 );
  }
}

void PanelVolume::OnSliderWindow( int nVal )
{
  QList<LayerMRI*> layers = GetSelectedLayers<LayerMRI*>();
  LayerMRI* curLayer = GetCurrentLayer<LayerMRI*>();
  if (!curLayer)
    return;
  double* r = curLayer->GetProperty()->GetWindowRange();
  foreach (LayerMRI* layer, layers)
  {
    layer->GetProperty()->SetWindow( nVal / 100.0 * ( r[1] - r[0] ) + r[0] );
  }
}

void PanelVolume::OnSliderLevel( int nVal )
{
  QList<LayerMRI*> layers = GetSelectedLayers<LayerMRI*>();
  LayerMRI* curLayer = GetCurrentLayer<LayerMRI*>();
  if (!curLayer)
    return;
  double* r = curLayer->GetProperty()->GetLevelRange();
  foreach (LayerMRI* layer, layers)
  {
    layer->GetProperty()->SetLevel( nVal / 100.0 * ( r[1] - r[0] ) + r[0] );
  }
}

void PanelVolume::OnSliderMin( int nVal )
{
  QList<LayerMRI*> layers = GetSelectedLayers<LayerMRI*>();
  LayerMRI* curLayer = GetCurrentLayer<LayerMRI*>();
  if (!curLayer)
    return;
  double fMin = curLayer->GetProperty()->GetMinValue();
  double fMax = curLayer->GetProperty()->GetMaxValue();
  double fScaleMin = fMin - (fMax-fMin)/4;
  double fScaleMax = fMax + (fMax-fMin)/4;
  foreach (LayerMRI* layer, layers)
  {
    switch ( layer->GetProperty()->GetColorMap() )
    {
    case LayerPropertyMRI::Grayscale:
      if (layer->GetProperty()->GetUsePercentile())
        layer->GetProperty()->SetMinGrayscaleWindow(layer->GetHistoValueFromPercentile(nVal/100.0));
      else
        layer->GetProperty()->SetMinGrayscaleWindow( nVal /
                                                     100.0 * ( fScaleMax - fScaleMin ) + fScaleMin );
      break;
    case LayerPropertyMRI::Heat:
      if (layer->GetProperty()->GetUsePercentile())
        layer->GetProperty()->SetHeatScaleMinThreshold(layer->GetHistoValueFromPercentile(nVal/100.0));
      else
        layer->GetProperty()->SetHeatScaleMinThreshold( nVal /
                                                        100.0 * ( fMax - fMin ) + fMin );
      break;
    default:
      if (layer->GetProperty()->GetUsePercentile())
        layer->GetProperty()->SetMinGenericThreshold(layer->GetHistoValueFromPercentile(nVal/100.0));
      else
        layer->GetProperty()->SetMinGenericThreshold( nVal /
                                                      100.0 * ( fScaleMax - fScaleMin ) + fScaleMin  );
      break;
    }
  }
}

void PanelVolume::OnSliderMid( int nVal )
{
  QList<LayerMRI*> layers = GetSelectedLayers<LayerMRI*>();
  LayerMRI* curLayer = GetCurrentLayer<LayerMRI*>();
  if (!curLayer)
    return;
  double fMin = curLayer->GetProperty()->GetMinValue();
  double fMax = curLayer->GetProperty()->GetMaxValue();
  foreach (LayerMRI* layer, layers)
  {
    if (layer->GetProperty()->GetUsePercentile())
      layer->GetProperty()->SetHeatScaleMidThreshold(layer->GetHistoValueFromPercentile(nVal/100.0));
    else
      layer->GetProperty()->SetHeatScaleMidThreshold( nVal / 100.0 * ( fMax - fMin ) + fMin );
  }
}

void PanelVolume::OnSliderMax( int nVal )
{
  QList<LayerMRI*> layers = GetSelectedLayers<LayerMRI*>();
  LayerMRI* curLayer = GetCurrentLayer<LayerMRI*>();
  if (!curLayer)
    return;
  double fMin = curLayer->GetProperty()->GetMinValue();
  double fMax = curLayer->GetProperty()->GetMaxValue();
  double fScaleMin = fMin - (fMax-fMin)/4;
  double fScaleMax = fMax + (fMax-fMin)/4;
  foreach (LayerMRI* layer, layers)
  {
    switch ( layer->GetProperty()->GetColorMap() )
    {
    case LayerPropertyMRI::Grayscale:
      if (layer->GetProperty()->GetUsePercentile())
        layer->GetProperty()->SetMaxGrayscaleWindow(layer->GetHistoValueFromPercentile(nVal/100.0));
      else
        layer->GetProperty()->SetMaxGrayscaleWindow( nVal /
                                                     100.0 * ( fScaleMax - fScaleMin ) + fScaleMin );
      break;
    case LayerPropertyMRI::Heat:
      if (layer->GetProperty()->GetUsePercentile())
        layer->GetProperty()->SetHeatScaleMaxThreshold(layer->GetHistoValueFromPercentile(nVal/100.0));
      else
        layer->GetProperty()->SetHeatScaleMaxThreshold( nVal /
                                                        100.0 * ( fMax - fMin ) + fMin );
      break;
    default:
      if (layer->GetProperty()->GetUsePercentile())
        layer->GetProperty()->SetMaxGenericThreshold(layer->GetHistoValueFromPercentile(nVal/100.0));
      else
        layer->GetProperty()->SetMaxGenericThreshold( nVal /
                                                      100.0 * ( fScaleMax - fScaleMin ) + fScaleMin  );
      break;
    }
  }
}

void PanelVolume::OnSliderOffset( int nVal )
{
  QList<LayerMRI*> layers = GetSelectedLayers<LayerMRI*>();
  LayerMRI* curLayer = GetCurrentLayer<LayerMRI*>();
  if (!curLayer)
    return;
  double fMax = curLayer->GetProperty()->GetMaxValue();
  foreach (LayerMRI* layer, layers)
  {
    layer->GetProperty()->SetHeatScaleOffset( nVal / 100.0 * ( fMax + fMax ) - fMax );
  }
}

void PanelVolume::OnLineEditWindow( const QString& text )
{
  QList<LayerMRI*> layers = GetSelectedLayers<LayerMRI*>();
  foreach (LayerMRI* layer, layers)
  {
    bool bOK;
    double dVal = text.toDouble( &bOK );
    if (bOK && layer->GetProperty()->GetWindow() != dVal )
    {
      layer->GetProperty()->SetWindow( dVal );
    }
  }
}

void PanelVolume::OnLineEditLevel( const QString& text )
{
  QList<LayerMRI*> layers = GetSelectedLayers<LayerMRI*>();
  foreach (LayerMRI* layer, layers)
  {
    bool bOK;
    double dVal = text.toDouble( &bOK );
    if ( bOK && layer->GetProperty()->GetLevel() != dVal )
    {
      layer->GetProperty()->SetLevel( dVal );
    }
  }
}

void PanelVolume::OnLineEditMin( const QString& text )
{
  QList<LayerMRI*> layers = GetSelectedLayers<LayerMRI*>();
  foreach (LayerMRI* layer, layers)
  {
    bool bOK;
    double dVal = text.toDouble( &bOK );
    if ( layer && bOK )
    {
      if (layer->GetProperty()->GetUsePercentile())
        dVal = layer->GetHistoValueFromPercentile(dVal/100.0);
      switch ( layer->GetProperty()->GetColorMap() )
      {
      case LayerPropertyMRI::Grayscale:
        layer->GetProperty()->SetMinGrayscaleWindow( dVal );
        break;
      case LayerPropertyMRI::Heat:
        layer->GetProperty()->SetHeatScaleMinThreshold( dVal );
        break;
      default:
        layer->GetProperty()->SetMinGenericThreshold( dVal );
        break;
      }
    }
  }
}

void PanelVolume::OnLineEditMid( const QString& text )
{
  QList<LayerMRI*> layers = GetSelectedLayers<LayerMRI*>();
  foreach (LayerMRI* layer, layers)
  {
    bool bOK;
    double dVal = text.toDouble( &bOK );
    if ( layer && bOK )
    {
      if (layer->GetProperty()->GetUsePercentile())
        dVal = layer->GetHistoValueFromPercentile(dVal/100.0);
      layer->GetProperty()->SetHeatScaleMidThreshold( dVal );
    }
  }
}

void PanelVolume::OnLineEditMax( const QString& text )
{
  QList<LayerMRI*> layers = GetSelectedLayers<LayerMRI*>();
  foreach (LayerMRI* layer, layers)
  {
    bool bOK;
    double dVal = text.toDouble( &bOK );
    if ( layer && bOK )
    {
      if (layer->GetProperty()->GetUsePercentile())
        dVal = layer->GetHistoValueFromPercentile(dVal/100.0);
      switch ( layer->GetProperty()->GetColorMap() )
      {
      case LayerPropertyMRI::Grayscale:
        layer->GetProperty()->SetMaxGrayscaleWindow( dVal );
        break;
      case LayerPropertyMRI::Heat:
        layer->GetProperty()->SetHeatScaleMaxThreshold( dVal );
        break;
      default:
        layer->GetProperty()->SetMaxGenericThreshold( dVal );
        break;
      }
    }
  }
}

void PanelVolume::OnLineEditOffset( const QString& text )
{
  QList<LayerMRI*> layers = GetSelectedLayers<LayerMRI*>();
  foreach (LayerMRI* layer, layers)
  {
    bool bOK;
    double dVal = text.toDouble( &bOK );
    if ( layer && bOK && layer->GetProperty()->GetHeatScaleOffset() != dVal )
    {
      layer->GetProperty()->SetHeatScaleOffset( dVal );
    }
  }
}

void PanelVolume::OnSliderContourMin(int nval)
{
  LayerMRI* layer = GetCurrentLayer<LayerMRI*>();
  if (!layer)
    return;
  double fMin = layer->GetProperty()->GetMinValue();
  double fMax = layer->GetProperty()->GetMaxValue();
  ChangeLineEditNumber( ui->lineEditContourThresholdLow,
                        nval / 100.0 * ( fMax - fMin ) + fMin );
}

void PanelVolume::OnSliderContourMax(int nval)
{
  LayerMRI* layer = GetCurrentLayer<LayerMRI*>();
  if (!layer)
    return;

  double fMin = layer->GetProperty()->GetMinValue();
  double fMax = layer->GetProperty()->GetMaxValue();
  ChangeLineEditNumber( ui->lineEditContourThresholdHigh,
                        nval / 100.0 * ( fMax - fMin ) + fMin );
}

void PanelVolume::OnSliderContourSmooth(int nval)
{
  ChangeLineEditNumber(ui->lineEditContourSmoothIteration, nval);
}

void PanelVolume::OnContourValueChanged()
{
  bool bOK;
  int nSmooth = 30;
  if (ui->checkBoxShowLabelContour->isChecked())
  {
    nSmooth = ui->lineEditContourSmoothIteration->text().trimmed().toInt(&bOK);
    QList<LayerMRI*> layers = GetSelectedLayers<LayerMRI*>();
    foreach (LayerMRI* layer, layers)
    {
      if (layer && bOK)
      {
        if (sender() == ui->lineEditContourSmoothIteration ||
            sender() == ui->sliderContourSmoothIteration )
        {
          layer->GetProperty()->SetContourSmoothIterations(nSmooth);
        }
        else
        {
          layer->GetProperty()->SetLabelContourRange(ui->lineEditContourLabelRange->text().trimmed());
        }
      }
    }
  }
  else
  {
    double fMin, fMax = 0;
    fMin = ui->lineEditContourThresholdLow->text().trimmed().toDouble(&bOK);
    if (bOK)
    {
      fMax = ui->lineEditContourThresholdHigh->text().trimmed().toDouble(&bOK);
    }
    if (bOK)
    {
      nSmooth = ui->lineEditContourSmoothIteration->text().trimmed().toInt(&bOK);
    }
    QList<LayerMRI*> layers = GetSelectedLayers<LayerMRI*>();
    foreach (LayerMRI* layer, layers)
    {
      if (layer && bOK)
      {
        if (sender() == ui->lineEditContourSmoothIteration ||
            sender() == ui->sliderContourSmoothIteration )
        {
          layer->GetProperty()->SetContourSmoothIterations(nSmooth);
        }
        else
        {
          layer->GetProperty()->SetContourThreshold(fMin, fMax);
        }
      }
    }
  }
}

void PanelVolume::OnContourSave()
{
  LayerMRI* layer = GetCurrentLayer<LayerMRI*>();
  if ( layer )
  {
    QString fn = QFileDialog::getSaveFileName( this,
                                               "Save iso-surface",
                                               MainWindow::GetMainWindow()->AutoSelectLastDir("mri") + "/" + layer->GetName() + ".vtk",
                                               "VTK files (*.vtk);;All files (*)");
    if ( !fn.isEmpty() )
    {
      if ( !layer->SaveContourToFile( fn ) )
      {
        QMessageBox::warning(this, "Error", "Can not save surface to file.");
      }
    }
  }
}

void PanelVolume::OnSliderTrackVolumeMin(int nval)
{
  LayerMRI* layer = GetCurrentLayer<LayerMRI*>();
  if ( layer && layer->IsTypeOf("VolumeTrack"))
  {
    double fMin = layer->GetProperty()->GetMinValue();
    double fMax = layer->GetProperty()->GetMaxValue()/4;
    ChangeLineEditNumber( ui->lineEditTrackVolumeThresholdLow,
                          nval / 100.0 * ( fMax - fMin ) + fMin );
  }
}

void PanelVolume::OnTrackVolumeThresholdChanged()
{
  QTreeWidgetItem* item = ui->treeWidgetColorTable->currentItem();
  if (!item)
    return;

  int nLabel = item->text(0).split(" ").at(0).toInt();
  bool bOK;
  double fMin;
  fMin = ui->lineEditTrackVolumeThresholdLow->text().trimmed().toDouble(&bOK);
  if (bOK)
  {
    LayerVolumeTrack* layer = GetCurrentLayer<LayerVolumeTrack*>();
    if (layer)
    {
      layer->SetThreshold(nLabel, fMin);
    }
  }
}

void PanelVolume::OnCopySettings()
{
  LayerMRI* layer = GetCurrentLayer<LayerMRI*>();
  if ( layer )
  {
    QVariantMap map = layer->GetProperty()->GetActiveSettings();
    QStringList keys = map.keys();
    QStringList strgs;
    strgs << FS_VOLUME_SETTING_ID;
    for (int i = 0; i < keys.size(); i++ )
    {
      strgs << keys[i] << map[keys[i]].toString();
    }
    qApp->clipboard()->setText(strgs.join(","));
  }
}

void PanelVolume::OnPasteSettings()
{
  LayerMRI* layer = GetCurrentLayer<LayerMRI*>();
  if ( layer )
  {
    QStringList strgs = qApp->clipboard()->text().split(",");
    if (!strgs.isEmpty() && strgs[0] == FS_VOLUME_SETTING_ID)
    {
      QVariantMap map;
      for (int i = 1; i < strgs.size(); i+=2 )
      {
        bool bOK;
        double val = strgs[i+1].toDouble(&bOK);
        if (bOK)
        {
          map[strgs[i]] = val;
        }
      }
      layer->GetProperty()->RestoreSettings(map);
    }
  }
}

void PanelVolume::OnPasteSettingsToAll()
{
  QStringList strgs = qApp->clipboard()->text().split(",");
  if (!strgs.isEmpty() && strgs[0] == FS_VOLUME_SETTING_ID)
  {
    QVariantMap map;
    for (int i = 1; i < strgs.size(); i+=2 )
    {
      bool bOK;
      double val = strgs[i+1].toDouble(&bOK);
      if (bOK)
      {
        map[strgs[i]] = val;
      }
    }
    QList<Layer*> layers = GetSelectedLayers<Layer*>();
    if (layers.size() < 2)
      layers = MainWindow::GetMainWindow()->GetLayerCollection("MRI")->GetLayers();
    for (int i = 0; i < layers.size(); i++)
    {
      ((LayerMRI*)layers[i])->GetProperty()->RestoreSettings(map);
    }
  }
}

void PanelVolume::OnActiveFrameChanged(int nFrame)
{
  LayerVolumeTrack* layer = GetCurrentLayer<LayerVolumeTrack*>();
  if ( layer )
  {
    int nLabel = layer->GetFrameLabel(nFrame);
    for (int i = 0; i < ui->treeWidgetColorTable->topLevelItemCount(); i++)
    {
      QTreeWidgetItem* item = ui->treeWidgetColorTable->topLevelItem(i);
      if ( item->text(0).split(" ").at(0).toInt() == nLabel )
      {
        ui->treeWidgetColorTable->setCurrentItem(item);
        return;
      }
    }
  }
}

void PanelVolume::OnShowExistingLabelsOnly(bool b)
{
  m_bShowExistingLabelsOnly = b;
  //  this->UpdateWidgets();
  OnLineEditBrushValue(ui->lineEditBrushValue->text());
}

void PanelVolume::OnComboMask(int sel)
{
  LayerMRI* mask = qobject_cast<LayerMRI*>(ui->comboBoxMask->itemData(sel).value<QObject*>());
  LayerMRI* layer = GetCurrentLayer<LayerMRI*>();
  if ( layer )
  {
    layer->SetMaskLayer(mask);
  }
}

void PanelVolume::OnComboCorrelationSurface(int nSel)
{
  LayerSurface* surf = qobject_cast<LayerSurface*>(ui->comboBoxCorrelationSurface->itemData(nSel).value<QObject*>());
  LayerMRI* layer = GetCurrentLayer<LayerMRI*>();
  if ( layer )
  {
    layer->SetCorrelationSurface(surf);
  }
}

void PanelVolume::OnCheckUsePercentile(bool b)
{
  QList<LayerMRI*> layers = GetSelectedLayers<LayerMRI*>();
  foreach (LayerMRI* layer, layers)
  {
    layer->GetProperty()->SetUsePercentile(b);
    UpdateWidgets();
  }
}

void PanelVolume::OnLockLayer(bool b)
{
  QList<LayerMRI*> layers = GetSelectedLayers<LayerMRI*>();
  foreach (LayerMRI* layer, layers)
  {
    layer->Lock(b);
  }
}

void PanelVolume::OnCheckBoxSetDisplayVector(bool b)
{
  QList<LayerMRI*> layers = GetSelectedLayers<LayerMRI*>();
  foreach (LayerMRI* layer, layers)
  {
    if (layer->GetNumberOfFrames() == 3 || layer->GetEndType() == "DTI")
      layer->GetProperty()->SetDisplayVector(b);
  }
}

void PanelVolume::OnCheckBoxSetDisplayTensor(bool b)
{
  QList<LayerMRI*> layers = GetSelectedLayers<LayerMRI*>();
  foreach (LayerMRI* layer, layers)
  {
    if (layer->GetNumberOfFrames() == 3)
      layer->GetProperty()->SetDisplayTensor(b);
  }
}

void PanelVolume::OnCheckBoxSetDisplayRGB(bool b)
{
  QList<LayerMRI*> layers = GetSelectedLayers<LayerMRI*>();
  foreach (LayerMRI* layer, layers)
  {
    layer->GetProperty()->SetDisplayRGB(b);
  }
}

void PanelVolume::OnCheckBoxSetNormalizeVector(bool b)
{
  QList<LayerMRI*> layers = GetSelectedLayers<LayerMRI*>();
  foreach (LayerMRI* layer, layers)
  {
    layer->GetProperty()->SetNormalizeVector(b);
  }
}

void PanelVolume::OnLineEditVectorDisplayScale(const QString &strg)
{
  bool ok;
  double val = strg.toDouble(&ok);
  if (ok && val > 0)
  {
    QList<LayerMRI*> layers = GetSelectedLayers<LayerMRI*>();
    foreach (LayerMRI* layer, layers)
    {
      layer->GetProperty()->SetVectorDisplayScale(val);
    }
  }
}

void PanelVolume::OnLineEditVectorLineWidth(const QString &strg)
{
  bool ok;
  double val = strg.toDouble(&ok);
  if (ok && val > 0)
  {
    QList<LayerMRI*> layers = GetSelectedLayers<LayerMRI*>();
    foreach (LayerMRI* layer, layers)
    {
      layer->GetProperty()->SetVectorLineWidth(val);
    }
  }
}

void PanelVolume::OnComboProjectionMapType(int nType)
{
  QList<LayerMRI*> layers = GetSelectedLayers<LayerMRI*>();
  foreach (LayerMRI* layer, layers)
  {
    layer->GetProperty()->SetProjectionMapType(nType);
  }
  UpdateWidgets();
}

void PanelVolume::OnLineEditProjectionMapRangeChanged()
{
  QList<LayerMRI*> layers = GetSelectedLayers<LayerMRI*>();
  foreach (LayerMRI* layer, layers)
  {
    QStringList list = ui->lineEditProjectionMapRange->text().trimmed().split(",", QString::SkipEmptyParts);
    if (list.size() < 2)
      list = ui->lineEditProjectionMapRange->text().trimmed().split(" ", QString::SkipEmptyParts);

    if (list.size() > 1)
    {
      int nRange[2] = {0, -1};
      for (int i = 0; i < 2; i++)
      {
        bool ok;
        nRange[i] = list[i].toInt(&ok);
        if (!ok)
        {
          UpdateWidgets();
          return;
        }
      }
      int nPlane = MainWindow::GetMainWindow()->GetMainViewId();
      if (nPlane > 2)
        nPlane = 2;
      int* dim = layer->GetImageData()->GetDimensions();
      if (nRange[0] < 0)
        nRange[0] = 0;
      if (nRange[1] >= dim[nPlane])
        nRange[1] = dim[nPlane]-1;
      if (nRange[1] >= nRange[0])
        layer->GetProperty()->SetProjectionMapRange(nPlane, nRange[0], nRange[1]);
    }
  }
  UpdateWidgets();
}

void PanelVolume::OnCheckBoxSelectAllLabels(int nState)
{
  ui->treeWidgetColorTable->blockSignals(true);
  if (nState == Qt::PartiallyChecked)
  {
    ui->checkBoxSelectAllLabels->blockSignals(true);
    ui->checkBoxSelectAllLabels->setCheckState(Qt::Checked);
    ui->checkBoxSelectAllLabels->blockSignals(false);
  }
  for ( int i = 0; i < ui->treeWidgetColorTable->topLevelItemCount(); i++ )
  {
    QTreeWidgetItem* item = ui->treeWidgetColorTable->topLevelItem( i );
    item->setCheckState(0, nState == Qt::Unchecked ? Qt::Unchecked : Qt::Checked);
  }
  ui->treeWidgetColorTable->blockSignals(false);

  LayerMRI* layer = GetCurrentLayer<LayerMRI*>();
  if ( layer )
  {
    if (nState == Qt::Unchecked)
      layer->GetProperty()->SetUnselectAllLabels();
    else
      layer->GetProperty()->SetSelectAllLabels();
  }
}

void PanelVolume::OnColorTableItemChanged(QTreeWidgetItem *item)
{
  ui->checkBoxSelectAllLabels->blockSignals(true);
  ui->checkBoxSelectAllLabels->setCheckState(Qt::PartiallyChecked);
  LayerMRI* layer = GetCurrentLayer<LayerMRI*>();
  if ( layer )
  {
    int nVal = item->text(0).split(" ").at(0).toInt();
    layer->GetProperty()->SetSelectLabel(nVal, item->checkState(0) == Qt::Checked);
    ui->checkBoxSelectAllLabels->setCheckState(layer->GetProperty()->GetSelectedLabels().isEmpty()?Qt::Unchecked:Qt::PartiallyChecked);
  }

  ui->checkBoxSelectAllLabels->blockSignals(false);
}

void PanelVolume::OnLineEditMaskThreshold( const QString& text )
{
  LayerMRI* layer = GetCurrentLayer<LayerMRI*>();
  if (layer)
  {
    bool bOK;
    double dVal = text.toDouble( &bOK );
    if ( bOK )
    {
      layer->SetMaskThreshold(dVal);
    }
  }
}

void PanelVolume::OnCustomContextMenu(const QPoint &pt)
{
  if (sender() == ui->treeWidgetColorTable)
  {
    QMenu menu;
    QAction* act = new QAction(this);
    QTreeWidgetItem* item = ui->treeWidgetColorTable->itemAt(pt);
    if (item)
    {
      QStringList strglist = item->text( 0 ).split(" ");
      double val = strglist[0].toDouble();
      LayerMRI* layer = GetCurrentLayer<LayerMRI*>();
      if ( layer )
      {
        double pos[3];
        if (layer->GetLayerLabelCenter(val, pos))
        {
          act->setText("Go to Centroid");
          connect(act, SIGNAL(triggered()), SLOT(OnColorTableItemDoubleClicked()));
        }
        else
          act->setText("Label does not exist in volume");
        menu.addAction(act);
        menu.addSeparator();
      }
    }
    act = new QAction(this);
    act->setText("Sort By Index");
    act->setCheckable(true);
    act->setChecked(ColorTableItem::SortType == ColorTableItem::ST_VALUE);
    act->setProperty("sort_by", ColorTableItem::ST_VALUE);
    connect(act, SIGNAL(triggered(bool)), SLOT(OnColorTableSortingChanged()));
    menu.addAction(act);
    act = new QAction(this);
    act->setCheckable(true);
    act->setChecked(ColorTableItem::SortType == ColorTableItem::ST_NAME);
    act->setText("Sort by Name");
    act->setProperty("sort_by", ColorTableItem::ST_NAME);
    connect(act, SIGNAL(triggered(bool)), SLOT(OnColorTableSortingChanged()));
    menu.addAction(act);
    menu.exec(ui->treeWidgetColorTable->mapToGlobal(pt));
  }
}

void PanelVolume::OnButtonResetWindowLevel()
{
  QList<LayerMRI*> layers = GetSelectedLayers<LayerMRI*>();
  foreach (LayerMRI* layer, layers)
  {
    layer->GetProperty()->ResetWindowLevel();
  }
}
