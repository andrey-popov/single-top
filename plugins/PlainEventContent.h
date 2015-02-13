#pragma once

#include <UserCode/SingleTop/interface/Electron.h>
#include <UserCode/SingleTop/interface/Muon.h>
#include <UserCode/SingleTop/interface/Jet.h>
#include <UserCode/SingleTop/interface/EventID.h>
#include <UserCode/SingleTop/interface/PileUpInfo.h>
#include <UserCode/SingleTop/interface/GeneratorInfo.h>

#include <FWCore/Framework/interface/EDAnalyzer.h>
#include <FWCore/Framework/interface/Event.h>
#include <FWCore/ParameterSet/interface/ParameterSet.h>
#include <FWCore/ParameterSet/interface/ConfigurationDescriptions.h>
#include <FWCore/ParameterSet/interface/ParameterSetDescription.h>
#include <FWCore/Utilities/interface/InputTag.h>

#include <FWCore/ServiceRegistry/interface/Service.h>
#include <CommonTools/UtilAlgos/interface/TFileService.h>

#include <TTree.h>

#include <string>
#include <vector>


/**
 * \class PlainEventContent
 * \author Andrey Popov
 * \brief This CMSSW plugin saves events in a ROOT file using a very slim format
 * 
 * The plugin stores most of basic objects: muons, electrons, jets, MET. It saves their
 * four-momenta, isolation, b-tagging discriminators, various IDs, etc. Most of the properties are
 * defined in the source code, but the user can provide a number of arbitrary string-based selection
 * criteria, whose results are evaluated and saved.
 * 
 * Sctructure of the output file differs between data and simulation, with some additional branches
 * added in the latter case.
 * 
 * Read documentation for data members, especially, storeElectrons, storeMuons, storeJets, and
 * storeMETs, for further information.
 */
class PlainEventContent: public edm::EDAnalyzer
{
public:
    /**
     * \brief Constructor
     * 
     * Initialises input tags, selections, and flags according to a given configuration. Allocates
     * arrays to keep results of additional selections (as the number of these selections is known
     * at run-time only).
     */
    PlainEventContent(edm::ParameterSet const &cfg);
    
public:
    /// A method to verify plugin's configuration
    static void fillDescriptions(edm::ConfigurationDescriptions &descriptions);
    
    /// Creates output trees and assigns branches to them
    virtual void beginJob();
    
    /**
     * \brief Analyses current event
     * 
     * Writes all the relevant information into buffers associated with the output trees and fills
     * the trees.
     */
    virtual void analyze(edm::Event const &event, edm::EventSetup const &setup);
    
private:
    /// Tags to access collections of electrons, muons, and jets
    edm::InputTag const electronTag, muonTag, jetTag;
    
    /**
     * \brief Tags to access collections of MET
     * 
     * The plugin reads not a single MET but a vector of them. It allows to store MET with various
     * corrections as well as its systematical variations.
     */
    std::vector<edm::InputTag> const metTags;
    
    /// Minimal corrected transverse momentum to determine which jets are stored
    double const jetMinPt;
    
    /// Minimal raw transverse momentum to determine which jets are stored
    double const jetMinRawPt;
    
    /**
     * \brief String-based selection whose result is to be saved
     * 
     * These selections do not affect which objects are stored in the output files. Instead, each
     * string defines a selection that is evalueated and whose result is saved in the bit field of
     * the CandidateWithID class.
     */
    std::vector<std::string> const eleSelection, muSelection, jetSelection;
    
    /**
     * \brief Indicates whether an event is data or simulation
     * 
     * It is used to deduce if the plugin should read generator information.
     */
    bool const runOnData;
    
    /**
     * \brief Tags to access generator information
     * 
     * They are ignored in case of real data.
     */
    edm::InputTag const generatorTag;
    
    /// Tag to access reconstructed primary vertices
    edm::InputTag const primaryVerticesTag;
    
    /// Pile-up information in simulation
    edm::InputTag const puSummaryTag;
    
    /// Rho (mean angular pt density)
    edm::InputTag const rhoTag;
    
    
    /// An object to handle the output ROOT file
    edm::Service<TFileService> fileService;
    
    
    /**
     * \brief Output tree
     * 
     * The tree aggregates all information stored by the plugin. Its structure differs between data
     * and simulation as in the latter case a branch with generator-level information is added.
     */
    TTree *outTree;
    
    /// Event ID
    pec::EventID eventId;
    
    /**
     * \brief An auxiliary pointer
     * 
     * ROOT needs a variable with a pointer to an object to store the object in a tree.
     */
    pec::EventID *eventIdPointer;
    
    /**
     * \brief Trimmed electrons to be stored in the output file
     * 
     * Mass is always close to the PDG value and thus does not encode useful information. It is set
     * to zero to faciliate compression. Bit flags include conversion rejection, trigger-emulating
     * preselection required for the triggering MVA ID [1-2], and user-defined selections. Consult
     * the source code to find their indices.
     * [1] https://twiki.cern.ch/twiki/bin/view/CMS/MultivariateElectronIdentification#Training_of_the_MVA
     * [2] https://hypernews.cern.ch/HyperNews/CMS/get/egamma-elecid/72.html
     */
    std::vector<pec::Electron> storeElectrons;
    
    /**
     * \brief An auxiliary pointer
     * 
     * ROOT needs a variable with a pointer to an object to store the object in a tree.
     */
    std::vector<pec::Electron> *storeElectronsPointer;
    
    /**
     * \brief Trimmed muons to be stored in the output file
     * 
     * Mass is always close to the PDG value and thus does not encode useful information. It is set
     * to zero to faciliate compression. Bit flags include the tight quality ID and user-defined
     * selections. Consult the source code to find their indices.
     */
    std::vector<pec::Muon> storeMuons;
    
    /**
     * \brief An auxiliary pointer
     * 
     * ROOT needs a variable with a pointer to an object to store the object in a tree.
     */
    std::vector<pec::Muon> *storeMuonsPointer;
    
    /**
     * \brief Trimmed jets to the stored in the output file
     * 
     * The four-momenta stored are uncorrected. In case of soft jets some properties might be set to
     * zero as they are not needed and this would allow a better compression in the output file. Bit
     * flags show if the jet is matched to a generator-level jet (always set to false in real data)
     * and include user-defined selections. Consult the source code to find the indices.
     */
    std::vector<pec::Jet> storeJets;
    
    /**
     * \brief An auxiliary pointer
     * 
     * ROOT needs a variable with a pointer to an object to store the object in a tree.
     */
    std::vector<pec::Jet> *storeJetsPointer;
        
    /**
     * \brief METs to be stored in the output file
     * 
     * Includes all METs whose input tags were provided to the plugin in the didecated parameter.
     * Usually, these are METs with different corrections and/or systematical variations. MET is
     * stored as an instance of pec::Candidate, but pseudorapidity and mass are set to zeros, which
     * allows them to be compressed efficiently.
     */
    std::vector<pec::Candidate> storeMETs;
    
    /**
     * \brief An auxiliary pointer
     * 
     * ROOT needs a variable with a pointer to an object to store the object in a tree.
     */
    std::vector<pec::Candidate> *storeMETsPointer;
    
    
    /// Basic generator information to be stored in the output file
    pec::GeneratorInfo generatorInfo;
    
    /**
     * \brief An auxiliary pointer
     * 
     * ROOT needs a variable with a pointer to an object to store the object in a tree.
     */
    pec::GeneratorInfo *generatorInfoPointer;
    
    
    /// Information on pile-up to be stored in the output file
    pec::PileUpInfo puInfo;
    
    /**
     * \brief An auxiliary pointer
     * 
     * ROOT needs a variable with a pointer to an object to store the object in a tree.
     */
    pec::PileUpInfo *puInfoPointer;
};
