// Copyright 2019-2020 CERN and copyright holders of ALICE O2.
// See https://alice-o2.web.cern.ch/copyright for details of the copyright holders.
// All rights not expressly granted are reserved.
//
// This software is distributed under the terms of the GNU General Public
// License v3 (GPL Version 3), copied verbatim in the file "COPYING".
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

///
/// \file   DigitsQcTask.cxx
/// \author Diego Stocco
/// \author Andrea Ferrero
///

#include <TCanvas.h>
#include <TH1.h>
#include <TH2.h>
#include <TFile.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "QualityControl/QcInfoLogger.h"
#include "MID/DigitsQcTask.h"
#include <Framework/InputRecord.h>

#include "Framework/DataRefUtils.h"
#include "DataFormatsMID/ColumnData.h"
#include "DataFormatsMID/ROBoard.h"
#include "DataFormatsMID/ROFRecord.h"
#include "DPLUtils/DPLRawParser.h"
#include "MIDQC/RawDataChecker.h"
#include "MIDRaw/CrateMasks.h"
#include "MIDRaw/Decoder.h"
#include "MIDRaw/ElectronicsDelay.h"
#include "MIDRaw/FEEIdConfig.h"
#include "MIDBase/Mapping.h"
#include "MIDBase/DetectorParameters.h"
#include "MIDBase/GeometryParameters.h"

#define MID_NDE 72
#define MID_NCOL 7

namespace o2::quality_control_modules::mid
{

DigitsQcTask::~DigitsQcTask()
{
}

void DigitsQcTask::initialize(o2::framework::InitContext& /*ctx*/)
{
  ILOG(Info, Support) << "initialize DigitsQcTask" << ENDM; // QcInfoLogger is used. FairMQ logs will go to there as well.

  // Histograms to be published
  mHitsMapB = new TH2F("HitsMapB", "Hits Map - bending plane", MID_NDE, 0, MID_NDE, MID_NCOL, 0, MID_NCOL);
  mHitsMapB->GetXaxis()->SetTitle("DE ID");
  mHitsMapB->GetYaxis()->SetTitle("Column ID");
  mHitsMapB->SetOption("colz");

  mHitsMapNB = new TH2F("HitsMapNB", "Hits Map - non-bending plane", MID_NDE, 0, MID_NDE, MID_NCOL, 0, MID_NCOL);
  mHitsMapNB->GetXaxis()->SetTitle("DE ID");
  mHitsMapNB->GetYaxis()->SetTitle("Column ID");
  mHitsMapNB->SetOption("colz");

  getObjectsManager()->startPublishing(mHitsMapB);
  getObjectsManager()->startPublishing(mHitsMapNB);

  mOrbitsMapB = new TH2F("OrbitsMapB", "Orbits Map - bending plane", MID_NDE, 0, MID_NDE, MID_NCOL, 0, MID_NCOL);
  mOrbitsMapB->GetXaxis()->SetTitle("DE ID");
  mOrbitsMapB->GetYaxis()->SetTitle("Column ID");
  mOrbitsMapB->SetOption("colz");

  mOrbitsMapNB = new TH2F("OrbitsMapNB", "Orbits Map - non-bending plane", MID_NDE, 0, MID_NDE, MID_NCOL, 0, MID_NCOL);
  mOrbitsMapNB->GetXaxis()->SetTitle("DE ID");
  mOrbitsMapNB->GetYaxis()->SetTitle("Column ID");
  mOrbitsMapNB->SetOption("colz");

  getObjectsManager()->startPublishing(mOrbitsMapB);
  getObjectsManager()->startPublishing(mOrbitsMapNB);

  mOccupancyMapB = new MergeableTH2Ratio("OccupancyMapB", "Occupancy - bending (MHz)", mHitsMapB, mOrbitsMapB);
  mOccupancyMapB->SetOption("colz");

  mOccupancyMapNB = new MergeableTH2Ratio("OccupancyMapNB", "Occupancy - non-bending (MHz)", mHitsMapNB, mOrbitsMapNB);
  mOccupancyMapNB->SetOption("colz");

  getObjectsManager()->startPublishing(mOccupancyMapB);
  getObjectsManager()->startPublishing(mOccupancyMapNB);

  mROFSizeB = new TH1F("ROFSizeB", "ROF size distribution - bending plane", 100, 0, 100);
  mROFSizeNB = new TH1F("ROFSizeNB", "ROF size distribution - non-bending plane", 100, 0, 100);

  getObjectsManager()->startPublishing(mROFSizeB);
  getObjectsManager()->startPublishing(mROFSizeNB);

  mROFTimeDiff = new TH2F("ROFTimeDiff", "ROF time difference vs. min. ROF size", 100, 0, 100, 100, 0, 100);
  mROFTimeDiff->SetOption("colz");

  getObjectsManager()->startPublishing(mROFTimeDiff);

  /////////////////

  mMultHitMT11B = new TH1F("MultHitMT11B", "Multiplicity Hits - MT11 bending plane", 300, 0, 300);
  mMultHitMT11NB = new TH1F("MultHitMT11NB", "Multiplicity Hits - MT11 non-bending plane", 300, 0, 300);
  mMultHitMT12B = new TH1F("MultHitMT12B", "Multiplicity Hits - MT12 bending plane", 300, 0, 300);
  mMultHitMT12NB = new TH1F("MultHitMT12NB", "Multiplicity Hits - MT12 non-bending plane", 300, 0, 300);
  mMultHitMT21B = new TH1F("MultHitMT21B", "Multiplicity Hits - MT21 bending plane", 300, 0, 300);
  mMultHitMT21NB = new TH1F("MultHitMT21NB", "Multiplicity Hits - MT21 non-bending plane", 300, 0, 300);
  mMultHitMT22B = new TH1F("MultHitMT22B", "Multiplicity Hits - MT22 bending plane", 300, 0, 300);
  mMultHitMT22NB = new TH1F("MultHitMT22NB", "Multiplicity Hits - MT22 non-bending plane", 300, 0, 300);

  mLocalBoardsMap = new TH2F("LocalBoardsMap", "Local boards Occupancy Map", 14, -7, 7, 36, 0, 9);
  mLocalBoardsMap->SetOption("colz");
  // mLocalBoardsMap->SetGrid();

  getObjectsManager()->startPublishing(mMultHitMT11B);
  getObjectsManager()->startPublishing(mMultHitMT11NB);
  getObjectsManager()->startPublishing(mMultHitMT12B);
  getObjectsManager()->startPublishing(mMultHitMT12NB);
  getObjectsManager()->startPublishing(mMultHitMT21B);
  getObjectsManager()->startPublishing(mMultHitMT21NB);
  getObjectsManager()->startPublishing(mMultHitMT22B);
  getObjectsManager()->startPublishing(mMultHitMT22NB);
  getObjectsManager()->startPublishing(mLocalBoardsMap);
}

void DigitsQcTask::startOfActivity(Activity& /*activity*/)
{
  ILOG(Info, Support) << "startOfActivity" << ENDM;
}

void DigitsQcTask::startOfCycle()
{
  ILOG(Info, Support) << "startOfCycle" << ENDM;
}

static int countColumnDataHits(const o2::mid::ColumnData& digit, int id)
{
  int nHits = 0;
  int mask = 32768; // 1000000000000000
  if (digit.patterns[id] != 0) {
    for (int j = 0; j < 16; j++) {
      if ((digit.patterns[id] & mask) != 0) {
        nHits += 1;
      }
      mask >>= 1;
    }
  }
  return nHits;
}

static int getBendingHits(const o2::mid::ColumnData& digit)
{
  int nHits = 0;
  for (int i = 0; i < 4; i++) {
    nHits += countColumnDataHits(digit, i);
  }
  return nHits;
}

static int getNonBendingHits(const o2::mid::ColumnData& digit)
{
  return countColumnDataHits(digit, 4);
}

static std::pair<uint32_t, uint32_t> getROFSize(const o2::mid::ROFRecord& rof, gsl::span<const o2::mid::ColumnData> digits)
{
  uint32_t nHitsB{ 0 };
  uint32_t nHitsNB{ 0 };

  auto lastEntry = rof.getEndIndex();
  for (auto i = rof.firstEntry; i < lastEntry; i++) {
    const auto& digit = digits[i];
    nHitsB += getBendingHits(digit);
    nHitsNB += getNonBendingHits(digit);
  }

  return std::make_pair(nHitsB, nHitsNB);
}

void DigitsQcTask::monitorData(o2::framework::ProcessingContext& ctx)
{

  auto digits = ctx.inputs().get<gsl::span<o2::mid::ColumnData>>("digits");
  auto rofs = ctx.inputs().get<gsl::span<o2::mid::ROFRecord>>("digitrofs");

  int multHitMT11B = 0;
  int multHitMT12B = 0;
  int multHitMT21B = 0;
  int multHitMT22B = 0;
  int multHitMT11NB = 0;
  int multHitMT12NB = 0;
  int multHitMT21NB = 0;
  int multHitMT22NB = 0;

  for (int i = 0; i < MID_NDE; i++) {
    for (int j = 0; j < MID_NCOL; j++) {
      mOrbitsMapB->Fill(i, j, 128);
      mOrbitsMapNB->Fill(i, j, 128);
    }
  }

  for (const auto& digit : digits) {
    // total number of bending plane hits
    int nHitsB = getBendingHits(digit);
    mHitsMapB->Fill(digit.deId, digit.columnId, nHitsB);
    // total number of non-bending plane hits
    int nHitsNB = getNonBendingHits(digit);
    mHitsMapNB->Fill(digit.deId, digit.columnId, nHitsNB);
  }

  std::pair<uint32_t, uint32_t> prevSize;
  o2::InteractionRecord prevIr;
  for (size_t i = 0; i < rofs.size(); i++) {
    const auto& rof = rofs[i];
    auto rofSize = getROFSize(rof, digits);
    mROFSizeB->Fill(rofSize.first);
    mROFSizeNB->Fill(rofSize.second);

    if (i > 0) {
      uint32_t sizeTot = rofSize.first + rofSize.second;
      uint32_t prevSizeTot = prevSize.first + prevSize.second;
      uint32_t sizeMin = (sizeTot < prevSizeTot) ? sizeTot : prevSizeTot;

      auto timeDiff = rof.interactionRecord.differenceInBC(prevIr);

      mROFTimeDiff->Fill(timeDiff, sizeMin);
    }

    prevSize = rofSize;
    prevIr = rof.interactionRecord;
  }

  //////////////////////////////
  o2::mid::Mapping mMapping;

  for (const auto& rofRecord : rofs) { // loop ROFRecords //
    nROF++;
    multHitMT11B = 0;
    multHitMT12B = 0;
    multHitMT21B = 0;
    multHitMT22B = 0;
    multHitMT11NB = 0;
    multHitMT12NB = 0;
    multHitMT21NB = 0;
    multHitMT22NB = 0;

    // loadStripPatterns (ColumnData)
    for (auto& digit : digits.subspan(rofRecord.firstEntry, rofRecord.nEntries)) { // loop DE //
      int deIndex = digit.deId;
      int colId = digit.columnId;
      int rpcLine = o2::mid::detparams::getRPCLine(deIndex);
      int ichamber = o2::mid::detparams::getChamber(deIndex);
      auto isRightSide = o2::mid::detparams::isRightSide(deIndex);
      auto detId = o2::mid::detparams::getDEId(isRightSide, ichamber, colId);
      for (int i = 0; i < 4; i++) {
        if (digit.patterns[i] != 0) {
          if (ichamber == 0) {
            int nBoard = 2;
            if (rpcLine == 0 || rpcLine == 8 || colId == 6)
              nBoard = 4;
            else if ((rpcLine == 3 || rpcLine == 5) && (colId == 0))
              nBoard = 1;
            else if ((rpcLine == 3 || rpcLine == 4 || rpcLine == 5) && (colId == 1 || colId == 2))
              nBoard = 1;
            double linepos = 0;
            int il;
            for (int board = 0; board < nBoard; board++) {
              linepos = rpcLine;
              il = i;
              if ((nBoard == 2) && (i == 1)) {
                linepos = rpcLine + 0.5;
                il = 0;
              }
              linepos = linepos + 0.01 + (0.25 * board) + (0.25 * il);
              if (isRightSide)
                mLocalBoardsMap->Fill(colId + 0.5, linepos, 1);
              else
                mLocalBoardsMap->Fill(-colId - 0.5, linepos, 1);
            }
          }
        }
      }

      if (ichamber == 0) {
        multHitMT11B += getBendingHits(digit);
        multHitMT11NB += getNonBendingHits(digit);
      } else if (ichamber == 1) {
        multHitMT12B += getBendingHits(digit);
        multHitMT12NB += getNonBendingHits(digit);
      } else if (ichamber == 2) {
        multHitMT21B += getBendingHits(digit);
        multHitMT21NB += getNonBendingHits(digit);
      } else if (ichamber == 3) {
        multHitMT22B += getBendingHits(digit);
        multHitMT22NB += getNonBendingHits(digit);
      }

    } // digits //
    mMultHitMT11B->Fill(multHitMT11B);
    mMultHitMT12B->Fill(multHitMT12B);
    mMultHitMT21B->Fill(multHitMT21B);
    mMultHitMT22B->Fill(multHitMT22B);
    mMultHitMT11NB->Fill(multHitMT11NB);
    mMultHitMT12NB->Fill(multHitMT12NB);
    mMultHitMT21NB->Fill(multHitMT21NB);
    mMultHitMT22NB->Fill(multHitMT22NB);
  } //  ROFRecords //

  //////////////////////////////
}

void DigitsQcTask::endOfCycle()
{
  ILOG(Info, Support) << "endOfCycle" << ENDM;

  mOccupancyMapB->update();
  mOccupancyMapNB->update();
}

void DigitsQcTask::endOfActivity(Activity& /*activity*/)
{
  ILOG(Info, Support) << "endOfActivity" << ENDM;
}

void DigitsQcTask::reset()
{
  // clean all the monitor objects here

  ILOG(Info, Support) << "Resetting the histogram" << ENDM;
}

} // namespace o2::quality_control_modules::mid
