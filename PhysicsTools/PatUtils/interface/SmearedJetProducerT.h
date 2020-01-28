#ifndef PAT_SMEAREDJETPRODUCERT_H
#define PAT_SMEAREDJETPRODUCERT_H

/** \class SmearedJetProducerT
 *
 * Produce collection of "smeared" jets.
 *
 * The aim of this correction is to account for the difference in jet energy resolution
 * between Monte Carlo simulation and Data.
 *
 * \author Sébastien Brochet
 *
 */

#include "CommonTools/Utils/interface/PtComparator.h"

#include "FWCore/Framework/interface/ConsumesCollector.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/stream/EDProducer.h"
#include "FWCore/ParameterSet/interface/ConfigurationDescriptions.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"
#include "FWCore/Utilities/interface/InputTag.h"

#include "DataFormats/JetReco/interface/GenJet.h"
#include "DataFormats/JetReco/interface/GenJetCollection.h"
#include "DataFormats/Math/interface/deltaR.h"

#include "JetMETCorrections/Modules/interface/JetResolution.h"

#include <TFile.h>
#include <TH1.h>
#include <TH1F.h>
#include <TRandom3.h>

#include <memory>
#include <random>

namespace pat {
class GenJetMatcher {
public:
    GenJetMatcher(const edm::ParameterSet& cfg, edm::ConsumesCollector&& collector)
        : m_genJetsToken(collector.consumes<reco::GenJetCollection>(cfg.getParameter<edm::InputTag>("genJets")))
        , m_dR_max(cfg.getParameter<double>("dRMax"))
        , m_dPt_max_factor(cfg.getParameter<double>("dPtMaxFactor"))
    {
        // Empty
    }

    static void fillDescriptions(edm::ParameterSetDescription& desc)
    {
        desc.add<edm::InputTag>("genJets");
        desc.add<double>("dRMax");
        desc.add<double>("dPtMaxFactor");
    }

    void getTokens(const edm::Event& event)
    {
        event.getByToken(m_genJetsToken, m_genJets);
    }

    template <class T>
    const reco::GenJet* match(const T& jet, double resolution)
    {
        const reco::GenJetCollection& genJets = *m_genJets;

        // Try to find a gen jet matching
        // dR < m_dR_max
        // dPt < m_dPt_max_factor * resolution

        double min_dR = std::numeric_limits<double>::infinity();
        const reco::GenJet* matched_genJet = nullptr;

        for (const auto& genJet : genJets) {
            double dR = deltaR(genJet, jet);

            if (dR > min_dR)
                continue;

            if (dR < m_dR_max) {
                double dPt = std::abs(genJet.pt() - jet.pt());
                if (dPt > m_dPt_max_factor * resolution)
                    continue;

                min_dR = dR;
                matched_genJet = &genJet;
            }
        }

        return matched_genJet;
    }

private:
    edm::EDGetTokenT<reco::GenJetCollection> m_genJetsToken;
    edm::Handle<reco::GenJetCollection> m_genJets;

    double m_dR_max;
    double m_dPt_max_factor;
};
};

template <typename T>
class SmearedJetProducerT : public edm::stream::EDProducer<> {

    using JetCollection = std::vector<T>;

public:
    explicit SmearedJetProducerT(const edm::ParameterSet& cfg)
        : m_enabled(cfg.getParameter<bool>("enabled"))
        , m_useDeterministicSeed(cfg.getParameter<bool>("useDeterministicSeed"))
        , m_debug(cfg.getUntrackedParameter<bool>("debug", false))
    {

        m_jets_token = consumes<JetCollection>(cfg.getParameter<edm::InputTag>("src"));

        if (m_enabled) {
            m_rho_token = consumes<double>(cfg.getParameter<edm::InputTag>("rho"));

            m_use_txt_files = cfg.exists("resolutionFile") && cfg.exists("scaleFactorFile");

            if (m_use_txt_files) {
                std::string resolutionFile = cfg.getParameter<edm::FileInPath>("resolutionFile").fullPath();
                std::string scaleFactorFile = cfg.getParameter<edm::FileInPath>("scaleFactorFile").fullPath();

                m_resolution_from_file.reset(new JME::JetResolution(resolutionFile));
                m_scale_factor_from_file.reset(new JME::JetResolutionScaleFactor(scaleFactorFile));
            } else {
                m_jets_algo = cfg.getParameter<std::string>("algo");
                m_jets_algo_pt = cfg.getParameter<std::string>("algopt");
            }

            std::uint32_t seed = cfg.getParameter<std::uint32_t>("seed");
            m_random_generator = std::mt19937(seed);
            m_random_generator_alt = TRandom3();

            bool skipGenMatching = cfg.getParameter<bool>("skipGenMatching");
            if (!skipGenMatching)
                m_genJetMatcher = std::make_shared<pat::GenJetMatcher>(cfg, consumesCollector());

            variation = cfg.getParameter<std::int32_t>("variation");
            m_nomVar = 1;

            if (variation == 0)
                m_systematic_variation = Variation::NOMINAL;
            else if (variation == 1)
                m_systematic_variation = Variation::UP;
            else if (variation == 2)
                m_systematic_variation = Variation::UP;
            else if (variation == 3)
                m_systematic_variation = Variation::UP;
            else if (variation == 4)
                m_systematic_variation = Variation::UP;
            else if (variation == 5)
                m_systematic_variation = Variation::UP;
            else if (variation == 6)
                m_systematic_variation = Variation::UP;
            else if (variation == 7)
                m_systematic_variation = Variation::UP;

            else if (variation == -1)
                m_systematic_variation = Variation::DOWN;
            else if (variation == -2)
                m_systematic_variation = Variation::DOWN;
            else if (variation == -3)
                m_systematic_variation = Variation::DOWN;
            else if (variation == -4)
                m_systematic_variation = Variation::DOWN;
            else if (variation == -5)
                m_systematic_variation = Variation::DOWN;
            else if (variation == -6)
                m_systematic_variation = Variation::DOWN;
            else if (variation == -7)
                m_systematic_variation = Variation::DOWN;

            else if (variation == 101) {
                m_systematic_variation = Variation::NOMINAL;
                m_nomVar = 1;
            } else if (variation == -101) {
                m_systematic_variation = Variation::NOMINAL;
                m_nomVar = -1;
            } else
                throw edm::Exception(edm::errors::ConfigFileReadError, "Invalid value for 'variation' parameter. Only -1, 0, 1 or 101, -101 are supported.");
        }

        produces<JetCollection>();
    }

    static void fillDescriptions(edm::ConfigurationDescriptions& descriptions)
    {
        edm::ParameterSetDescription desc;

        desc.add<edm::InputTag>("src");
        desc.add<bool>("enabled");
        desc.add<edm::InputTag>("rho");
        desc.add<std::int32_t>("variation", 0);
        desc.add<std::uint32_t>("seed", 37428479);
        desc.add<bool>("skipGenMatching", false);
        desc.add<bool>("useDeterministicSeed", true);
        desc.addUntracked<bool>("debug", false);

        auto source = (edm::ParameterDescription<std::string>("algo", true) and edm::ParameterDescription<std::string>("algopt", true)) xor (edm::ParameterDescription<edm::FileInPath>("resolutionFile", true) and edm::ParameterDescription<edm::FileInPath>("scaleFactorFile", true));
        desc.addNode(std::move(source));

        pat::GenJetMatcher::fillDescriptions(desc);

        descriptions.addDefault(desc);
    }

    virtual void produce(edm::Event& event, const edm::EventSetup& setup) override
    {

        edm::Handle<JetCollection> jets_collection;
        event.getByToken(m_jets_token, jets_collection);

        // Disable the module when running on real data
        if (m_enabled && event.isRealData()) {
            m_enabled = false;
            m_genJetMatcher.reset();
        }

        edm::Handle<double> rho;
        if (m_enabled)
            event.getByToken(m_rho_token, rho);

        JME::JetResolution resolution;
        JME::JetResolutionScaleFactor resolution_sf;

        // use non-const jetcollection compared to unchanged SmearedJetProducer to be able to add userfloats
        JetCollection jets = *jets_collection;

        if (m_enabled) {
            if (m_use_txt_files) {
                resolution = *m_resolution_from_file;
                resolution_sf = *m_scale_factor_from_file;
            } else {
                resolution = JME::JetResolution::get(setup, m_jets_algo_pt);
                resolution_sf = JME::JetResolutionScaleFactor::get(setup, m_jets_algo);
            }

            if (m_useDeterministicSeed) {
                unsigned int runNum_uint = static_cast<unsigned int>(event.id().run());
                unsigned int lumiNum_uint = static_cast<unsigned int>(event.id().luminosityBlock());
                unsigned int evNum_uint = static_cast<unsigned int>(event.id().event());
                unsigned int jet0eta = uint32_t(jets.empty() ? 0 : jets[0].eta() / 0.01);
                std::uint32_t seed = jet0eta + m_nomVar + (lumiNum_uint << 10) + (runNum_uint << 20) + evNum_uint;
                m_random_generator.seed(seed);
            }
        }

        if (m_genJetMatcher)
            m_genJetMatcher->getTokens(event);

        auto smearedJets = std::make_unique<JetCollection>();
        // therefore use non-const iterator over jets in jetcollection
        for (auto& jet : jets) {

            if ((!m_enabled) || (jet.pt() == 0)) {
                // Module disabled or invalid p4. Simply copy the input jet.
                smearedJets->push_back(jet);

                continue;
            }
            double smearFactor = GetCorrection(jet, m_systematic_variation, resolution, resolution_sf, *rho);
            double smearFactor_nom = GetCorrection(jet, Variation::NOMINAL, resolution, resolution_sf, *rho);



           
            switch (m_systematic_variation) {
            
            case Variation::NOMINAL:
                jet.addUserFloat("HelperJER", smearFactor);
                break;
            case Variation::UP:
                jet.addUserFloat("HelperJER", smearFactor_nom);
                jet.addUserFloat("HelperJERup", smearFactor / smearFactor_nom);
                break;

            case Variation::DOWN:
                jet.addUserFloat("HelperJER", smearFactor_nom);
                jet.addUserFloat("HelperJERdown", smearFactor / smearFactor_nom);
                break;
            }

            // std::cout << variation << std::endl;
            switch (variation) {

            // add JER SFs as user floats to jets to be able to use later
            // do sources according to eta bins 
            //   - |eta|<1.93,   1 pt bin [0,infty)           : JEReta0 (+-2)
            //   - 1.93<|eta|<2.5,  1 pt bin [0,infty)        : JEReta1 (+-3)
            //   - 2.5<|eta|<3, two pt bins [0,50],[50,infty) : JERpt0eta2 (+-4) JERpt1eta2 (+-5) 
            //   - 3<|eta|<5, two pt bins [0,50],[50,infty)   : JERpt0eta3 (+-6) JERpt1eta3 (+-7)
            
            case 2: // JEReta0up
                if (abs(jet.eta()) <= 1.93) {
                    jet.addUserFloat("HelperJEReta0up", smearFactor / smearFactor_nom);
                } 
                else {
                    smearFactor = 1.;
                    jet.addUserFloat("HelperJEReta0up", smearFactor / smearFactor_nom);
                }
                break; 
            case -2: // JEReta0down
                if (abs(jet.eta()) <= 1.93) {
                    jet.addUserFloat("HelperJEReta0down", smearFactor / smearFactor_nom);
                } 
                else {
                    smearFactor = 1.;
                    jet.addUserFloat("HelperJEReta0down", smearFactor / smearFactor_nom);
                }
                break; 
            case 3: // JEReta1up
                if (1.93 < abs(jet.eta()) and abs(jet.eta()) < 2.5) {
                    jet.addUserFloat("HelperJEReta1up", smearFactor / smearFactor_nom);
                } 
                else {
                    smearFactor = 1.;
                    jet.addUserFloat("HelperJEReta1up", smearFactor / smearFactor_nom);
                }
                break; 
            case -3: // JERpt0eta1down
                if (1.93 < abs(jet.eta()) and abs(jet.eta()) < 2.5) {
                    jet.addUserFloat("HelperJEReta1down", smearFactor / smearFactor_nom);
                } 
                else {
                    smearFactor = 1.;
                    jet.addUserFloat("HelperJEReta1down", smearFactor / smearFactor_nom);
                }
                break; 
            case 4: // JERpt0eta2up
                if (jet.pt() < 50  and 2.50 <= abs(jet.eta()) and abs(jet.eta()) < 3.0) {
                    jet.addUserFloat("HelperJERpt0eta2up", smearFactor / smearFactor_nom);
                } 
                else {
                    smearFactor = 1.;
                    jet.addUserFloat("HelperJERpt0eta2up", smearFactor / smearFactor_nom);
                }
                break; 
            case -4: // JERpt0eta2down
                if (jet.pt() < 50  and 2.50 <= abs(jet.eta()) and abs(jet.eta()) < 3.0) {
                    jet.addUserFloat("HelperJERpt0eta2down", smearFactor / smearFactor_nom);
                } 
                else {
                    smearFactor = 1.;
                    jet.addUserFloat("HelperJERpt0eta2down", smearFactor / smearFactor_nom);
                }
                break; 
            case 5: // JERpt1eta2up
                if (jet.pt() >= 50  and 2.50 <= abs(jet.eta()) and abs(jet.eta()) < 3.0) {
                    jet.addUserFloat("HelperJERpt1eta2up", smearFactor / smearFactor_nom);
                } 
                else {
                    smearFactor = 1.;
                    jet.addUserFloat("HelperJERpt1eta2up", smearFactor / smearFactor_nom);
                }
                break; 
            case -5: // JERpt1eta2down
                if (jet.pt() >= 50  and 2.50 <= abs(jet.eta()) and abs(jet.eta()) < 3.0) {
                    jet.addUserFloat("HelperJERpt1eta2down", smearFactor / smearFactor_nom);
                } 
                else {
                    smearFactor = 1.;
                    jet.addUserFloat("HelperJERpt1eta2down", smearFactor / smearFactor_nom);
                }
                break; 
            case 6: // JERpt0eta3up
                if (jet.pt() < 50  and 3.0 <= abs(jet.eta()) and abs(jet.eta()) < 5.0) {
                    jet.addUserFloat("HelperJERpt0eta3up", smearFactor / smearFactor_nom);
                } 
                else {
                    smearFactor = 1.;
                    jet.addUserFloat("HelperJERpt0eta3up", smearFactor / smearFactor_nom);
                }
                break; 
            case -6: // JERpt0eta3down
                if (jet.pt() < 50  and 3.0 <= abs(jet.eta()) and abs(jet.eta()) < 5.0) {
                    jet.addUserFloat("HelperJERpt0eta3down", smearFactor / smearFactor_nom);
                } 
                else {
                    smearFactor = 1.;
                    jet.addUserFloat("HelperJERpt0eta3down", smearFactor / smearFactor_nom);
                }
                break; 
            case 7: // JERpt1eta3up
                if (jet.pt() >= 50  and 3.0 <= abs(jet.eta()) and abs(jet.eta()) < 5.0) {
                    jet.addUserFloat("HelperJERpt1eta3up", smearFactor / smearFactor_nom);
                } 
                else {
                    smearFactor = 1.;
                    jet.addUserFloat("HelperJERpt1eta3up", smearFactor / smearFactor_nom);
                }
                break; 
            case -7: // JERpt1eta3down
                if (jet.pt() >= 50  and 3.0 <= abs(jet.eta()) and abs(jet.eta()) < 5.0) {
                    jet.addUserFloat("HelperJERpt1eta3down", smearFactor / smearFactor_nom);
                } 
                else {
                    smearFactor = 1.;
                    jet.addUserFloat("HelperJERpt1eta3down", smearFactor / smearFactor_nom);
                }
                break; 

            }

            T smearedJet = jet;
            smearedJet.scaleEnergy(smearFactor);

            if (m_debug) {
                std::cout << "smeared jet (" << smearFactor << "):  pt: " << smearedJet.pt() << "  eta: " << smearedJet.eta() << "  phi: " << smearedJet.phi() << "  e: " << smearedJet.energy() << std::endl;
            }

            smearedJets->push_back(smearedJet);
        }

        // Sort jets by pt
        std::sort(smearedJets->begin(), smearedJets->end(), jetPtComparator);

        event.put(std::move(smearedJets));
    }

    double GetCorrection(const auto& jet, const Variation& m_systematic_variation_, const JME::JetResolution& resolution, const JME::JetResolutionScaleFactor& resolution_sf, const double& rho)
    {
        double smearFactor = 1.;
        double jet_resolution = 1.;
        double jer_sf = 1.;
        jet_resolution = resolution.getResolution({ { JME::Binning::JetPt, jet.pt() }, { JME::Binning::JetEta, jet.eta() }, { JME::Binning::Rho, rho } });
        jer_sf = resolution_sf.getScaleFactor({ { JME::Binning::JetPt, jet.pt() }, { JME::Binning::JetEta, jet.eta() } }, m_systematic_variation_);

        if (m_debug) {
            std::cout << "jet:  pt: " << jet.pt() << "  eta: " << jet.eta() << "  phi: " << jet.phi() << "  e: " << jet.energy() << std::endl;
            std::cout << "resolution: " << jet_resolution << std::endl;
            std::cout << "resolution scale factor: " << jer_sf << std::endl;
        }

        const reco::GenJet* genJet = nullptr;
        if (m_genJetMatcher)
            genJet = m_genJetMatcher->match(jet, jet.pt() * jet_resolution);

        if (genJet) {
            /*
                    * Case 1: we have a "good" gen jet matched to the reco jet
                    */

            if (m_debug) {
                std::cout << "gen jet:  pt: " << genJet->pt() << "  eta: " << genJet->eta() << "  phi: " << genJet->phi() << "  e: " << genJet->energy() << std::endl;
            }

            double dPt = jet.pt() - genJet->pt();
            smearFactor = 1 + m_nomVar * (jer_sf - 1.) * dPt / jet.pt();

        } else if (jer_sf > 1) {
            /*
                    * Case 2: we don't have a gen jet. Smear jet pt using a random gaussian variation
                    */

            double sigma = jet_resolution * std::sqrt(jer_sf * jer_sf - 1);
            if (m_debug) {
                std::cout << "gaussian width: " << sigma << std::endl;
            }

            std::normal_distribution<> d(0, sigma);
            smearFactor = 1. + m_nomVar * d(m_random_generator);

            // if jet has deterministic seeds then use the Trandom3 geneerator and seed it with the corresponding deterministic seed
            if (jet.hasUserInt("deterministicSeed")) {
                m_random_generator_alt.SetSeed((uint32_t)jet.userInt("deterministicSeed"));
                smearFactor = 1. + m_nomVar * m_random_generator_alt.Gaus(0, sigma);
            }

        } else if (m_debug) {
            std::cout << "Impossible to smear this jet" << std::endl;
        }

        if (jet.energy() * smearFactor < MIN_JET_ENERGY) {
            // Negative or too small smearFactor. We would change direction of the jet
            // and this is not what we want.
            // Recompute the smearing factor in order to have jet.energy() == MIN_JET_ENERGY
            double newSmearFactor = MIN_JET_ENERGY / jet.energy();
            if (m_debug) {
                std::cout << "The smearing factor (" << smearFactor << ") is either negative or too small. Fixing it to " << newSmearFactor << " to avoid change of direction." << std::endl;
            }
            smearFactor = newSmearFactor;
        }
        return smearFactor;
    }

private:
    static constexpr const double MIN_JET_ENERGY = 1e-2;

    std::int32_t variation;

    edm::EDGetTokenT<JetCollection> m_jets_token;
    edm::EDGetTokenT<double> m_rho_token;
    bool m_enabled;
    std::string m_jets_algo_pt;
    std::string m_jets_algo;
    Variation m_systematic_variation;
    bool m_useDeterministicSeed;
    bool m_debug;
    std::shared_ptr<pat::GenJetMatcher> m_genJetMatcher;

    bool m_use_txt_files;
    std::unique_ptr<JME::JetResolution> m_resolution_from_file;
    std::unique_ptr<JME::JetResolutionScaleFactor> m_scale_factor_from_file;

    std::mt19937 m_random_generator;
    TRandom3 m_random_generator_alt;

    GreaterByPt<T> jetPtComparator;
    int m_nomVar;
};
#endif
