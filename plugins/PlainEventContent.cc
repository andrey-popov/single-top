#include "PlainEventContent.h"

#include <FWCore/Framework/interface/EventSetup.h>
#include <FWCore/Framework/interface/ESHandle.h>
#include <FWCore/Utilities/interface/EDMException.h>
#include <FWCore/Utilities/interface/InputTag.h>
#include <FWCore/Framework/interface/MakerMacros.h>

#include <CommonTools/Utils/interface/StringCutObjectSelector.h>

#include <TLorentzVector.h>
#include <Math/GenVector/VectorUtil.h>

#include <memory>
#include <cmath>
#include <sstream>


using namespace edm;
using namespace std;


PlainEventContent::PlainEventContent(edm::ParameterSet const &cfg):
    jetMinPt(cfg.getParameter<double>("jetMinPt")),
    jetMinRawPt(cfg.getParameter<double>("jetMinRawPt")),
    
    eleSelection(cfg.getParameter<vector<string>>("eleSelection")),
    muSelection(cfg.getParameter<vector<string>>("muSelection")),
    jetSelection(cfg.getParameter<vector<string>>("jetSelection")),
    
    runOnData(cfg.getParameter<bool>("runOnData"))
{
    electronToken = consumes<edm::View<pat::Electron>>(cfg.getParameter<InputTag>("electrons"));
    muonToken = consumes<edm::View<pat::Muon>>(cfg.getParameter<InputTag>("muons"));
    jetToken = consumes<edm::View<pat::Jet>>(cfg.getParameter<InputTag>("jets"));
    
    for (edm::InputTag const &tag: cfg.getParameter<vector<InputTag>>("METs"))
        metTokens.emplace_back(consumes<edm::View<pat::MET>>(tag));
    
    generatorToken = consumes<GenEventInfoProduct>(cfg.getParameter<InputTag>("generator"));
    primaryVerticesToken =
     consumes<reco::VertexCollection>(cfg.getParameter<InputTag>("primaryVertices"));
    puSummaryToken = consumes<edm::View<PileupSummaryInfo>>(cfg.getParameter<InputTag>("puInfo"));
    rhoToken = consumes<double>(cfg.getParameter<InputTag>("rho"));
}


void PlainEventContent::fillDescriptions(edm::ConfigurationDescriptions &descriptions)
{
    // Documentation for descriptions of the configuration is available in [1]
    //[1] https://twiki.cern.ch/twiki/bin/view/CMSPublic/SWGuideConfigurationValidationAndHelp
    
    edm::ParameterSetDescription desc;
    desc.add<bool>("runOnData")->
     setComment("Indicates whether data or simulation is being processed.");
    desc.add<InputTag>("primaryVertices")->
     setComment("Collection of reconstructed primary vertices.");
    desc.add<InputTag>("electrons")->setComment("Collection of electrons.");
    desc.add<vector<string>>("eleSelection", vector<string>(0))->
     setComment("User-defined selections for electrons whose results will be stored in the output "
     "tree.");
    desc.add<InputTag>("muons")->setComment("Collection of muons.");
    desc.add<vector<string>>("muSelection", vector<string>(0))->
     setComment("User-defined selections for muons whose results will be stored in the ouput "
     "tree.");
    desc.add<InputTag>("jets")->setComment("Collection of jets.");
    desc.add<vector<string>>("jetSelection", vector<string>(0))->
     setComment("User-defined selections for jets whose results will be stored in the output "
     "tree.");
    desc.add<double>("jetMinPt", 20.)->
     setComment("Jets with corrected pt above this threshold will be stored in the output tree.");
    desc.add<double>("jetMinRawPt", 10.)->
     setComment("Jets with raw pt above this threshold will be stored in the output tree.");
    desc.add<vector<InputTag>>("METs")->setComment("MET. Several versions of it can be stored.");
    desc.add<InputTag>("generator", InputTag("generator"))->
     setComment("Tag to access information about generator. If runOnData is true, this parameter "
     "is ignored.");
    desc.add<InputTag>("rho", InputTag("kt6PFJets", "rho"))->
     setComment("Rho (mean angular pt density).");
    desc.add<InputTag>("puInfo", InputTag("addPileupInfo"))->
     setComment("True pile-up information. If runOnData is true, this parameter is ignored.");
    
    descriptions.add("eventContent", desc);
}


void PlainEventContent::beginJob()
{
    // Create the output tree
    outTree = fileService->make<TTree>("EventContent", "Minimalistic description of events");
    
    
    // A branch with event ID
    eventIdPointer = &eventId;
    outTree->Branch("eventId", &eventIdPointer);
    
    
    // Branches with reconstucted objects
    storeElectronsPointer = &storeElectrons;
    outTree->Branch("electrons", &storeElectronsPointer);
    
    storeMuonsPointer = &storeMuons;
    outTree->Branch("muons", &storeMuonsPointer);
    
    storeJetsPointer = &storeJets;
    outTree->Branch("jets", &storeJetsPointer);
    
    storeMETsPointer = &storeMETs;
    outTree->Branch("METs", &storeMETsPointer);
    
    
    // A branch with most basic generator-level information
    if (!runOnData)
    {
        generatorInfoPointer = &generatorInfo;
        outTree->Branch("genInfo", &generatorInfoPointer);
    }
    
    
    // A branch with per-event information on pile-up
    puInfoPointer = &puInfo;
    outTree->Branch("puInfo", &puInfoPointer);
}


void PlainEventContent::analyze(edm::Event const &event, edm::EventSetup const &setup)
{
    // Fill the event ID tree
    eventId.Reset();
    
    eventId.SetRunNumber(event.id().run());
    eventId.SetEventNumber(event.id().event());
    eventId.SetLumiSectionNumber(event.luminosityBlock());
    
    
    // Read the primary vertices
    Handle<reco::VertexCollection> vertices;
    event.getByToken(primaryVerticesToken, vertices);
    
    if (vertices->size() == 0)
    {
        edm::Exception excp(edm::errors::LogicError);
        excp << "Event contains zero good primary vertices.\n";
        excp.raise();
    }
    
    
    // Fill the tree with basic information
    // Read the electron collection
    Handle<View<pat::Electron>> srcElectrons;
    event.getByToken(electronToken, srcElectrons);
    
    
    // Construct the electron selectors (s. SWGuidePhysicsCutParser)
    vector<StringCutObjectSelector<pat::Electron>> eleSelectors;
    
    for (vector<string>::const_iterator sel = eleSelection.begin(); sel != eleSelection.end();
     ++sel)
        eleSelectors.push_back(*sel);
    
    
    // Loop through the electron collection and fill the relevant variables
    storeElectrons.clear();
    pec::Electron storeElectron;  // will reuse this object to fill the vector
    
    for (unsigned i = 0; i < srcElectrons->size(); ++i)
    {
        pat::Electron const &el = srcElectrons->at(i);
        storeElectron.Reset();
        
        
        // Set four-momentum. Mass is ignored
        storeElectron.SetPt(el.ecalDrivenMomentum().pt());
        storeElectron.SetEta(el.ecalDrivenMomentum().eta());
        storeElectron.SetPhi(el.ecalDrivenMomentum().phi());
        //^ Gsf momentum is used instead of the one calculated by the particle-flow algorithm
        //https://twiki.cern.ch/twiki/bin/view/CMS/TWikiTopRefEventSel?rev=178#Electrons
        
        double const pt = el.ecalDrivenMomentum().pt();  // a short-cut to be used for isolation
        
        storeElectron.SetCharge(el.charge());
        storeElectron.SetDB(el.dB());
        
        // Effective-area (rho) correction to isolation [1]
        //[1] https://twiki.cern.ch/twiki/bin/viewauth/CMS/TwikiTopRefHermeticTopProjections
        storeElectron.SetRelIso((el.chargedHadronIso() + max(el.neutralHadronIso() +
         el.photonIso() - 1. * el.userIsolation("User1Iso"), 0.)) / pt);
        
        // Triggering MVA ID [1]
        //[1] https://twiki.cern.ch/twiki/bin/view/CMS/MultivariateElectronIdentification
        storeElectron.SetMvaID(el.electronID("mvaTrigV0"));
        
        // Cut-based electron ID [1]
        //[1] https://twiki.cern.ch/twiki/bin/view/CMS/SimpleCutBasedEleID
        storeElectron.SetCutBasedID(el.electronID("simpleEleId70cIso"));
        
        // Conversion rejection. True for a "good" electron
        storeElectron.SetBit(0, el.passConversionVeto());
        //^ See [1]. The decision is stored by PATElectronProducer based on the collection
        //"allConversions" (the name is hard-coded). In the past, there used to be an additional
        //requirement to reject electrons from the photon conversion is set according to [2]; but
        //it caused a compile error in 72X and has been dropped
        //[1] https://twiki.cern.ch/twiki/bin/view/CMS/ConversionTools
        //[2] https://twiki.cern.ch/twiki/bin/view/CMS/TWikiTopRefEventSel#Electrons
        
        
        // Evaluate user-defined selectors if any
        for (unsigned i = 0; i < eleSelectors.size(); ++i)
            storeElectron.SetBit(2 + i, eleSelectors[i](el));
        
        
        // The electron is set up. Add it to the vector
        storeElectrons.push_back(storeElectron);
    }
    
    
    // Read the muon collection
    Handle<View<pat::Muon>> srcMuons;
    event.getByToken(muonToken, srcMuons);
    
    // Constuct the muon selectors
    vector<StringCutObjectSelector<pat::Muon>> muSelectors;
    
    for (vector<string>::const_iterator sel = muSelection.begin(); sel != muSelection.end(); ++sel)
        muSelectors.push_back(*sel);
    
    
    // Loop through the muon collection and fill the relevant variables
    storeMuons.clear();
    pec::Muon storeMuon;  // will reuse this object to fill the vector
    
    for (unsigned i = 0; i < srcMuons->size(); ++i)
    {
        pat::Muon const &mu = srcMuons->at(i);
        storeMuon.Reset();
        
        
        // Set four-momentum. Mass is ignored
        storeMuon.SetPt(mu.pt());
        storeMuon.SetEta(mu.eta());
        storeMuon.SetPhi(mu.phi());
        
        storeMuon.SetCharge(mu.charge());
        storeMuon.SetDB(mu.dB());
        
        // Relative isolation with delta-beta correction. Logic of the calculation follows [1]. Note
        //that it is calculated differently from [2], but the adopted recipe is more natural for
        //PFBRECO
        //[1] http://cmssw.cvs.cern.ch/cgi-bin/cmssw.cgi/CMSSW/CommonTools/ParticleFlow/interface/IsolatedPFCandidateSelectorDefinition.h?revision=1.4&view=markup
        //[2] https://twiki.cern.ch/twiki/bin/view/CMSPublic/SWGuideMuonId#Accessing_PF_Isolation_from_reco
        storeMuon.SetRelIso((mu.chargedHadronIso() + max(mu.neutralHadronIso() + mu.photonIso() -
         0.5 * mu.puChargedHadronIso(), 0.)) / mu.pt());
        
        // Tight muons are defined according to [1]. Note it does not imply selection on isolation
        //or kinematics
        //[1] https://twiki.cern.ch/twiki/bin/view/CMSPublic/SWGuideMuonId?rev=48#Tight_Muon
        storeMuon.SetBit(0, mu.isTightMuon(vertices->front()));
        
        // Evaluate user-defined selectors if any
        for (unsigned i = 0; i < muSelectors.size(); ++i)
            storeMuon.SetBit(2 + i, muSelectors[i](mu));
        
        
        // The muon is set up. Add it to the vector
        storeMuons.push_back(storeMuon);
    }
    
    
    // Read the jets collections
    Handle<View<pat::Jet>> srcJets;
    event.getByToken(jetToken, srcJets);
    
    
    // Construct the jet selectors
    vector<StringCutObjectSelector<pat::Jet>> jetSelectors;
    
    for (vector<string>::const_iterator sel = jetSelection.begin(); sel != jetSelection.end();
     ++sel)
        jetSelectors.push_back(*sel);
    
    
    // Loop through the jet collection and fill the relevant variables
    storeJets.clear();
    pec::Jet storeJet;  // will reuse this object to fill the vector
    
    for (unsigned int i = 0; i < srcJets->size(); ++i)
    {
        pat::Jet const &j = srcJets->at(i);
        reco::Candidate::LorentzVector const &rawP4 = j.correctedP4("Uncorrected");
        storeJet.Reset();
        
        if (j.pt() > jetMinPt or rawP4.pt() > jetMinRawPt)
        {
            // Set four-momentum
            storeJet.SetPt(rawP4.pt());
            storeJet.SetEta(rawP4.eta());
            storeJet.SetPhi(rawP4.phi());
            storeJet.SetM(rawP4.mass());
            
            
            storeJet.SetArea(j.jetArea());
            storeJet.SetCharge(j.jetCharge());
            
            storeJet.SetBTagCSV(j.bDiscriminator("combinedSecondaryVertexBJetTags"));
            storeJet.SetBTagTCHP(j.bDiscriminator("trackCountingHighPurBJetTags"));
            
            
            // Calculate the secondary vertex mass [1-3]
            //[1] https://twiki.cern.ch/twiki/bin/view/CMSPublic/WorkBookPATExampleTrackBJet#ExerCise5
            //[2] https://hypernews.cern.ch/HyperNews/CMS/get/btag/718/1.html
            //[3] https://hypernews.cern.ch/HyperNews/CMS/get/physTools/2714.html
            double secVertexMass = -100.;
            reco::SecondaryVertexTagInfo const *svTagInfo = j.tagInfoSecondaryVertex();
            
            if (svTagInfo and svTagInfo->nVertices() > 0)
                secVertexMass = svTagInfo->secondaryVertex(0).p4().mass();
            
            storeJet.SetSecVertexMass(secVertexMass);
            
            
            // Calculate the jet pull angle
            double const y = rawP4.Rapidity();
            double const phi = rawP4.phi();
            double pullY = 0., pullPhi = 0.;  // projections of the pull vector (unnormalised)
            
            // Loop over constituents of the jet
            for (reco::PFCandidatePtr const &p: j.getPFConstituents())
            {
                double dPhi = p->phi() - phi;
                
                if (dPhi < -TMath::Pi())
                    dPhi = 2 * TMath::Pi() + dPhi;
                else if (dPhi > TMath::Pi())
                    dPhi = -2 * TMath::Pi() + dPhi;
                
                double const r = hypot(p->rapidity() - y, dPhi);
                pullY += p->pt() * r * (p->rapidity() - y);
                pullPhi += p->pt() * r * dPhi;
            }
            //^ The pull vector should be normalised by the jet's pt, but since I'm interested in
            //the polar angle only, it is not necessary
            
            storeJet.SetPullAngle(atan2(pullPhi, pullY));
            
            

            if (!runOnData)
            // These are variables is from the generator tree, but it's more convenient to
            //calculate it here
            {
                storeJet.SetFlavour(pec::Jet::FlavourType::Algorithmic, j.partonFlavour());
                storeJet.SetFlavour(pec::Jet::FlavourType::Physics,
                 (j.genParton() == nullptr) ? 0 : j.genParton()->pdgId());
                
                storeJet.SetBit(0, (j.genJet() and j.genJet()->pt() > 8. and
                 ROOT::Math::VectorUtil::DeltaR(j.p4(), j.genJet()->p4()) < 0.25));
                //^ The matching is performed according to the definition from JME-13-005. By
                //default, PAT uses a looser definition
            }
            
            
            // User-difined selectors if any. The first bit has already been used for the match with
            //generator-level jet
            for (unsigned i = 0; i < jetSelectors.size(); ++i)
                storeJet.SetBit(i + 1, jetSelectors[i](j));
            
            
            // The jet is set up. Add it to the vector
            storeJets.push_back(storeJet);
        }
    }
    
    
    // Read METs
    storeMETs.clear();
    pec::Candidate storeMET;  // will reuse this object to fill the vector
    
    for (auto const &metToken: metTokens)
    {
        Handle<View<pat::MET>> met;
        event.getByToken(metToken, met);
        
        storeMET.Reset();
        
        storeMET.SetPt(met->front().pt());
        storeMET.SetPhi(met->front().phi());
        
        storeMETs.push_back(storeMET);
    }
    
    
    // Save the generator information (however the jet generator info is already saved)
    // Save the PDF and other generator information
    if (!runOnData)
    {
        Handle<GenEventInfoProduct> generator;
        event.getByToken(generatorToken, generator);
        
        generatorInfo.Reset();
        //^ Same object is used for all events, hence need to reset it
        
        generatorInfo.SetProcessId(generator->signalProcessID());
        generatorInfo.SetWeight(generator->weight());
        
        
        GenEventInfoProduct::PDF const *pdf = generator->pdf();
        
        if (pdf)
        {
            generatorInfo.SetPdfXs(pdf->x.first, pdf->x.second);
            generatorInfo.SetPdfIds(pdf->id.first, pdf->id.second);
            generatorInfo.SetPdfQScale(pdf->scalePDF);
        }
    }
    
    
    // Save the pile-up information
    puInfo.Reset();
    //^ Same object is used for all events, hence need to reset it
    
    puInfo.SetNumPV(vertices->size());

    Handle<double> rho;
    event.getByToken(rhoToken, rho);
    
    puInfo.SetRho(*rho);
    
    
    if (!runOnData)
    {
        Handle<View<PileupSummaryInfo>> puSummary;
        event.getByToken(puSummaryToken, puSummary);
        
        puInfo.SetTrueNumPU(puSummary->front().getTrueNumInteractions());
        //^ The true number of interactions is same for all bunch crossings
        
        for (unsigned i = 0; i < puSummary->size(); ++i)
            if (puSummary->at(i).getBunchCrossing() == 0)
            {
                puInfo.SetInTimePU(puSummary->at(i).getPU_NumInteractions());
                break;
            }
    }
    
    
    // Fill the output tree
    outTree->Fill();
}


DEFINE_FWK_MODULE(PlainEventContent);
