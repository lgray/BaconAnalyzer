//================================================================================================
//
// Perform preselection for W/Zprime(->qq)+jets events and produce bacon bits 
//
// Input arguments
//   argv[1] => lName = input bacon file name
//   argv[2] => lOption = dataset type: "mc", "data"
//   argv[3] => lJSON = JSON file for run-lumi filtering of data, specify "none" for MC or no filtering
//   argv[4] => lXS = cross section (pb), ignored for data 
//   argv[5] => weight = total weight, ignored for data
//________________________________________________________________________________________________

#include "../include/GenLoader.hh"
#include "../include/EvtLoader.hh"
#include "../include/ElectronLoader.hh"
#include "../include/MuonLoader.hh"
#include "../include/PhotonLoader.hh"
#include "../include/TauLoader.hh"
#include "../include/JetLoader.hh"
#include "../include/VJetLoader.hh"
#include "../include/RunLumiRangeMap.h"

#include "TROOT.h"
#include "TFile.h"
#include "TTree.h"
#include <TError.h>
#include <string>
#include <iostream>

// Object Processors
GenLoader       *fGen        = 0; 
EvtLoader       *fEvt        = 0; 
MuonLoader      *fMuon       = 0; 
ElectronLoader  *fElectron   = 0; 
TauLoader       *fTau        = 0; 
PhotonLoader    *fPhoton     = 0; 
JetLoader       *fJet4       = 0;
VJetLoader      *fVJet8      = 0;
RunLumiRangeMap *fRangeMap   = 0; 

TH1F *fHist                  = 0;


const int NUM_PDF_WEIGHTS = 60;

// Load tree and return infile
TTree* load(std::string iName) { 
  TFile *lFile = TFile::Open(iName.c_str());
  TTree *lTree = (TTree*) lFile->FindObjectAny("Events");
  fHist        = (TH1F* ) lFile->FindObjectAny("TotalEvents");
  return lTree;
}

// For Json 
bool passEvent(unsigned int iRun,unsigned int iLumi) { 
  RunLumiRangeMap::RunLumiPairType lRunLumi(iRun,iLumi);
  return fRangeMap->HasRunLumi(lRunLumi);
}

// === MAIN =======================================================================================================
int main( int argc, char **argv ) {
  gROOT->ProcessLine("#include <vector>");
  const std::string lName        = argv[1];
  const std::string lOption      = argv[2];
  const std::string lJSON        = argv[3];
  const double      lXS          = atof(argv[4]);
  //const double      weight       = atof(argv[5]);
  const int      iSplit          = atoi(argv[6]);
  const int      maxSplit        = atoi(argv[7]);

  fRangeMap = new RunLumiRangeMap();
  if(lJSON.size() > 0) fRangeMap->AddJSONFile(lJSON.c_str());

  bool isData;
  if(lOption.compare("data")!=0) isData = false;
  else isData = true;
				   
  TTree *lTree = load(lName); 
  // Declare Readers 
  fEvt       = new EvtLoader     (lTree,lName);                                             // fEvt, fEvtBr, fVertices, fVertexBr
  fMuon      = new MuonLoader    (lTree);                                                   // fMuon and fMuonBr, fN = 2 - muonArr and muonBr
  fElectron  = new ElectronLoader(lTree);                                                   // fElectrons and fElectronBr, fN = 2
  fTau       = new TauLoader     (lTree);                                                   // fTaus and fTaurBr, fN = 1
  fPhoton    = new PhotonLoader  (lTree);                                                   // fPhotons and fPhotonBr, fN = 1
  fJet4      = new JetLoader     (lTree, isData);                                           // fJets, fJetBr => AK4PUPPI, sorted by pT
  fVJet8     = new VJetLoader    (lTree,"AK8Puppi","AddAK8Puppi",3, isData,true);           // fVJets, fVJetBr => AK8PUPPI
  if(lOption.compare("data")!=0) fGen      = new GenLoader     (lTree);                     // fGenInfo, fGenInfoBr => GenEvtInfo, fGens and fGenBr => GenParticle

  TFile *lFile = TFile::Open("Output.root","RECREATE");
  TTree *lOut  = new TTree("Events","Events");

  //Setup histograms containing total number of processed events (for normalization)
  TH1F *NEvents = new TH1F("NEvents", "NEvents", 1, 0.5, 1.5);
  TH1F *SumWeights = new TH1F("SumWeights", "SumWeights", 1, 0.5, 1.5);
  TH1F *SumScaleWeights = new TH1F("SumScaleWeights", "SumScaleWeights", 6, -0.5, 5.5);
  TH1F *SumPdfWeights = new TH1F("SumPdfWeights", "SumPdfWeights", NUM_PDF_WEIGHTS, -0.5, NUM_PDF_WEIGHTS-0.5);
  
    
  // Setup Tree
  fEvt      ->setupTree      (lOut); 
  fVJet8    ->setupTree      (lOut,"AK8Puppijet",true);  // Hww
  fJet4     ->setupTree      (lOut,"AK4Puppijet");
  fMuon     ->setupTree      (lOut); 
  fElectron ->setupTree      (lOut); 
  fTau      ->setupTree      (lOut);
  if(lOption.compare("data")!=0) {
    fGen ->setupTree (lOut,float(lXS));
    fGen ->setupTreeHiggs(lOut);
  }

  // Loop over events i0 = iEvent
  int neventstest = 0;
  int neventsTotal = int(lTree->GetEntriesFast());
  int minEventsPerJob = neventsTotal / maxSplit;
  int minEvent = iSplit * minEventsPerJob;
  int maxEvent = (iSplit+1) * minEventsPerJob;
  // int nwhad(0), nwlep(0);
  // int nwhadR(0), nwlepR(0);
  // int nwhadC(0), nwlepC(0);
  // int nhbb(0), nhWW(0), nhZZ(0), nhcc(0), nhqq(0), nhtt(0), nhgg(0), nhpp(0);
  if (iSplit + 1 == maxSplit) maxEvent = neventsTotal;
  std::cout << neventsTotal << " total events" << std::endl;
  std::cout << iSplit << " iSplit " << std::endl;
  std::cout << maxSplit << " maxSplit " << std::endl;
  std::cout << minEvent << " min event" << std::endl;
  std::cout << maxEvent << " max event" << std::endl;  
  for(int i0 = minEvent; i0 < maxEvent; i0++) {
    //for(int i0 = 0; i0 < int(10000); i0++){ // for testing
    if (i0%1000 == 0) std::cout << i0 << " events processed " << std::endl;
    // Check GenInfo
    fEvt->load(i0);
    float lWeight = 1;
    unsigned int passJson = 0;
    if(lOption.compare("data")!=0){
      fGen->load(i0);
      lWeight = fGen->fWeight;
      passJson = 1;
    }
    else{
      if(passEvent(fEvt->fRun,fEvt->fLumi)) passJson = 1;
    }
    
    NEvents->SetBinContent(1, NEvents->GetBinContent(1)+lWeight);
    SumWeights->Fill(1.0, lWeight);

    // Primary vertex requirement
    if(!fEvt->PV()) continue;
    
    // Triggerbits
    unsigned int trigbits=1;   

    if(fEvt ->passTrigger("HLT_AK8PFJet360_TrimMass30_v*") ||
       fEvt ->passTrigger("HLT_AK8PFHT700_TrimR0p1PT0p03Mass50_v*") ||
       fEvt ->passTrigger("HLT_PFHT800_v*") || 
       fEvt ->passTrigger("HLT_PFHT900_v*") || 
       fEvt ->passTrigger("HLT_PFHT650_WideJetMJJ950DEtaJJ1p5_v*") ||
       fEvt ->passTrigger("HLT_PFHT650_WideJetMJJ900DEtaJJ1p5_v*") ||
       fEvt ->passTrigger("HLT_AK8DiPFJet280_200_TrimMass30_BTagCSV_p20_v*") || 
       fEvt ->passTrigger("HLT_PFJet450_v*") ||
       fEvt ->passTrigger("HLT_AK8PFHT750_TrimMass50_v*") ||
       fEvt ->passTrigger("HLT_AK8PFHT800_TrimMass50_v*") ||
       fEvt ->passTrigger("HLT_AK8PFHT850_TrimMass50_v*") ||
       fEvt ->passTrigger("HLT_AK8PFHT900_TrimMass50_v*") ||
       fEvt ->passTrigger("HLT_AK8PFJet400_TrimMass30_v*") ||
       fEvt ->passTrigger("HLT_AK8PFJet500_v*") ||
       fEvt ->passTrigger("HLT_PFJet500_v*") ||
       fEvt ->passTrigger("HLT_AK8PFJetFwd450_v*")
       )  {
      trigbits = trigbits | 2;  // hadronic signal region 2016
    }
    if( fEvt ->passTrigger("HLT_Mu50_v*") ||
	fEvt ->passTrigger("HLT_TkMu50_v*")
	) { trigbits = trigbits | 4; 
    } // single muon control region
    if( fEvt ->passTrigger("HLT_Ele45_WPLoose_v*") ||
	fEvt ->passTrigger("HLT_Ele105_CaloIdVT_GsfTrkIdT_v*")
	) { trigbits = trigbits | 8; // single electron control region
    }
    fEvt      ->fillEvent(trigbits,lWeight,passJson);
    
    // Objects
    gErrorIgnoreLevel=kError;
    std::vector<TLorentzVector> cleaningMuons, cleaningElectrons, cleaningPhotons; 
    std::vector<TLorentzVector> Muons, Electrons;
    fMuon     ->load(i0);
    fMuon     ->selectMuons(Muons,fEvt->fMet,fEvt->fMetPhi);
    fElectron ->load(i0);
    fElectron ->selectElectrons(fEvt->fRho,fEvt->fMet,Electrons);
    fTau      ->load(i0);
    fTau      ->selectTaus(Electrons, Muons);    
    fPhoton   ->load(i0);
    fPhoton   ->selectPhotons(fEvt->fRho,cleaningElectrons,cleaningPhotons);

    // AK8Puppi Jets    
    fVJet8    ->load(i0);
    fVJet8    ->selectVJets(cleaningElectrons,cleaningMuons,cleaningPhotons,0.8,fEvt->fRho,fEvt->fRun,true);

    if(lName.find("BulkGrav")!=std::string::npos){
      for (int i0=0; i0<int(fVJet8->selectedVJets.size()); i0++){
	fVJet8->fisMatchedVJet[i0] = fGen->isHDau(25, 24, fVJet8->selectedVJets[i0], 2);
      }
    }
    
    // AK4Puppi Jets
    fJet4     ->load(i0);
    fJet4     ->selectJets(cleaningElectrons,cleaningMuons,cleaningPhotons,fVJet8->selectedVJets,fEvt->fRho,fEvt->fRun);

    lOut->Fill();
    neventstest++;
  }

  std::cout << neventstest << std::endl;
  std::cout << lTree->GetEntriesFast() << std::endl;
  lFile->cd();
  lOut->Write();  
  NEvents->Write();
  SumWeights->Write();
  SumScaleWeights->Write();
  SumPdfWeights->Write();
  lFile->Close();
}