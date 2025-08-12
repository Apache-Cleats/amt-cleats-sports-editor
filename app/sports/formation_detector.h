#ifndef FORMATION_DETECTOR_H
#define FORMATION_DETECTOR_H

/**
 * @file formation_detector.h
 * @brief AI-powered formation detection for Triangle Defense analysis
 * 
 * Computer vision and machine learning engine for real-time detection
 * of football formations using Triangle Defense methodology.
 * 
 * Integrates with M.E.L. AI system for intelligent coaching analysis.
 */

#include "sports_analysis_core.h"
#include <opencv2/opencv.hpp>
#include <vector>
#include <memory>

namespace amt {
namespace sports {

/**
 * @struct PlayerPosition
 * @brief Represents detected player position on field
 */
struct PlayerPosition {
    cv::Point2f position;      // X,Y coordinates on field
    int jersey_number;         // Player identification
    std::string team;          // "offense" or "defense"
    double confidence;         // Detection confidence 0.0-1.0
    cv::Rect bounding_box;     // Player bounding box
};

/**
 * @struct FieldGeometry
 * @brief Field positioning and camera perspective data
 */
struct FieldGeometry {
    cv::Point2f yard_lines[11];    // Yard line positions in frame
    cv::Point2f hash_marks[4];     // Hash mark positions
    cv::Point2f sidelines[2];      // Sideline boundaries
    double pixels_per_yard;        // Scale factor
    cv::Mat homography_matrix;     // Perspective transformation
};

/**
 * @struct MOAnalysis
 * @brief Middle of 5 (MO) analysis for Triangle Defense
 */
struct MOAnalysis {
    cv::Point2f mo_position;       // Middle of 5 offensive eligibles
    std::vector<PlayerPosition> eligibles;  // 5 eligible receivers
    FormationType recommended_formation;    // Suggested Triangle Defense response
    std::string tactical_note;     // Coaching recommendation
};

/**
 * @class FormationDetector
 * @brief AI-powered formation detection and analysis engine
 */
class FormationDetector {
public:
    FormationDetector();
    ~FormationDetector();
    
    // Core Detection Functions
    bool initialize(const std::string& model_path = "");
    FormationData detectFormation(const cv::Mat& frame);
    std::vector<PlayerPosition> detectPlayers(const cv::Mat& frame);
    FieldGeometry calibrateField(const cv::Mat& frame);
    
    // Triangle Defense Analysis
    MOAnalysis analyzeMO(const std::vector<PlayerPosition>& players);
    FormationType classifyTriangleDefense(const std::vector<PlayerPosition>& defense);
    CLSAnalysis performAdvancedCLS(const FormationData& formation, const cv::Mat& frame);
    
    // Real-time Processing
    void startLiveAnalysis();
    void stopLiveAnalysis();
    void processVideoStream(cv::VideoCapture& capture);
    
    // Configuration
    void setDetectionThreshold(double threshold);
    void enableFormationType(FormationType type, bool enabled);
    void setFieldDimensions(int yard_width, int yard_height);
    
    // M.E.L. AI Integration
    void connectToMELAI();
    void sendAnalysisToMEL(const FormationData& data);
    void receiveCoachingRecommendations();
    
    // Coaching Tools
    cv::Mat annotateFrame(const cv::Mat& frame, const FormationData& formation);
    cv::Mat createTacticalDiagram(const FormationData& formation);
    void exportFormationReport(const std::string& filename);
    
    // Performance Monitoring
    double getProcessingFPS() const;
    int getFramesProcessed() const;
    void resetStatistics();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
    
    // Internal Processing
    std::vector<cv::Point2f> detectKeyPoints(const cv::Mat& frame);
    cv::Mat preprocessFrame(const cv::Mat& input);
    bool validateDetection(const FormationData& formation);
    
    // Triangle Defense Logic
    FormationType identifyLarryFormation(const std::vector<PlayerPosition>& defense);
    FormationType identifyLindaFormation(const std::vector<PlayerPosition>& defense);
    FormationType identifyRitaFormation(const std::vector<PlayerPosition>& defense);
    FormationType identifyRickyFormation(const std::vector<PlayerPosition>& defense);
    FormationType identifyLeonFormation(const std::vector<PlayerPosition>& defense);
    FormationType identifyRandyFormation(const std::vector<PlayerPosition>& defense);
    FormationType identifyPatFormation(const std::vector<PlayerPosition>& defense);
    
    // AI Model Integration
    bool loadDetectionModel(const std::string& model_path);
    cv::Mat runInference(const cv::Mat& input);
    void updateModelWeights();
};

/**
 * @class LiveFormationTracker
 * @brief Real-time formation tracking for live game analysis
 */
class LiveFormationTracker {
public:
    LiveFormationTracker(FormationDetector* detector);
    ~LiveFormationTracker();
    
    void startTracking();
    void stopTracking();
    void processFrame(const cv::Mat& frame, double timestamp);
    
    // Timeline Integration
    std::vector<FormationData> getFormationHistory();
    FormationData getFormationAtTime(double timestamp);
    void exportTimelineData(const std::string& filename);
    
    // Coaching Interface
    void setPlayBoundaries(double start_time, double end_time);
    std::vector<FormationData> analyzePlay(double start_time, double end_time);
    void generatePlayReport(double start_time, double end_time);

private:
    class TrackerImpl;
    std::unique_ptr<TrackerImpl> m_tracker_impl;
};

// Utility Functions
namespace formation_utils {
    cv::Point2f calculateCenterOfMass(const std::vector<PlayerPosition>& players);
    double calculateFormationWidth(const std::vector<PlayerPosition>& players);
    double calculateFormationDepth(const std::vector<PlayerPosition>& players);
    bool isTriangleDefenseFormation(const std::vector<PlayerPosition>& defense);
    std::string formationToString(FormationType type);
    FormationType stringToFormation(const std::string& name);
}

} // namespace sports
} // namespace amt

#endif // FORMATION_DETECTOR_H
