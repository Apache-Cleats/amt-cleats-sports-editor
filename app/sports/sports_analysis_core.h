#ifndef SPORTS_ANALYSIS_CORE_H
#define SPORTS_ANALYSIS_CORE_H

/**
 * @file sports_analysis_core.h
 * @brief Core sports analysis engine for AMT Cleats Football Platform
 * 
 * This file contains the foundation for Triangle Defense methodology
 * and M.E.L. AI integration for real-time football analysis.
 * 
 * Part of AnalyzeMyTeam (Company 01) - Flagship Football Intelligence Empire
 */

#include <memory>
#include <vector>
#include <string>

namespace amt {
namespace sports {

enum class FormationType {
    LARRY,   // 5-3-1 Formation variant
    LINDA,   // 5-3-1 Formation variant  
    RITA,    // 5-3-1 Formation variant
    RICKY,   // 5-3-1 Formation variant
    LEON,    // 5-3-1 Formation variant
    RANDY,   // 5-3-1 Formation variant
    PAT,     // 5-3-1 Formation variant
    UNKNOWN
};

struct FormationData {
    FormationType type;
    double confidence;
    std::string description;
    int frame_number;
    double timestamp;
};

struct CLSAnalysis {
    std::string configuration;  // Formation configuration
    std::string location;       // Field position analysis
    std::string situation;      // Game situation context
    double cls_score;          // Overall CLS framework score
};

/**
 * @class SportsAnalysisCore
 * @brief Main engine for Triangle Defense analysis and M.E.L. AI coordination
 */
class SportsAnalysisCore {
public:
    SportsAnalysisCore();
    ~SportsAnalysisCore();
    
    // Triangle Defense Analysis
    FormationData analyzeFormation(const void* video_frame);
    CLSAnalysis performCLSAnalysis(const FormationData& formation);
    
    // M.E.L. AI Integration
    void connectToMELAI();
    void syncWithAnalyzeMyTeam();
    
    // Coaching Tools
    std::vector<std::string> generateCoachingInsights(const FormationData& formation);
    void exportCoachingClip(double start_time, double end_time);
    
private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace sports
} // namespace amt

#endif // SPORTS_ANALYSIS_CORE_H
