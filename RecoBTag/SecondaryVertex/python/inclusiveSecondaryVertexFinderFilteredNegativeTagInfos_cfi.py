import FWCore.ParameterSet.Config as cms

from RecoBTag.SecondaryVertex.inclusiveSecondaryVertexFinderFilteredTagInfos_cfi import *

inclusiveSecondaryVertexFinderFilteredNegativeTagInfos = inclusiveSecondaryVertexFinderFilteredTagInfos.clone()
inclusiveSecondaryVertexFinderFilteredNegativeTagInfos.extSVDeltaRToJet = cms.double(-0.4)
inclusiveSecondaryVertexFinderFilteredNegativeTagInfos.vertexCuts.distVal2dMin = -2.5
inclusiveSecondaryVertexFinderFilteredNegativeTagInfos.vertexCuts.distVal2dMax = -0.01
inclusiveSecondaryVertexFinderFilteredNegativeTagInfos.vertexCuts.distSig2dMin = -99999.9
inclusiveSecondaryVertexFinderFilteredNegativeTagInfos.vertexCuts.distSig2dMax = -2.0
inclusiveSecondaryVertexFinderFilteredNegativeTagInfos.vertexCuts.maxDeltaRToJetAxis = -0.5
