/***
  Apache-Cleats Sports Editor
  Copyright (C) 2024 AnalyzeMyTeam

  Formation Overlay Rendering System
  Real-time Triangle Defense formation visualization over video content
***/

#ifndef FORMATIONOVERLAY_H
#define FORMATIONOVERLAY_H

#include <QObject>
#include <QWidget>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QGraphicsPixmapItem>
#include <QGraphicsTextItem>
#include <QGraphicsLineItem>
#include <QGraphicsEllipseItem>
#include <QGraphicsPathItem>
#include <QTimer>
#include <QMutex>
#include <QQueue>
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>
#include <QSequentialAnimationGroup>
#include <QPainter>
#include <QPainterPath>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QElapsedTimer>

#include "triangle_defense_sync.h"

namespace olive {

// Forward declarations
class SequenceViewerPanel;
class TimelinePanel;

/**
 * @brief Formation overlay rendering modes
 */
enum class OverlayMode {
  Disabled,           // No overlay rendering
  FormationsOnly,     // Show only formation detection
  TriangleCalls,      // Show Triangle Defense calls
  FullAnalysis,       // Show formations + calls + M.E.L. data
  CoachingMode,       // Simplified view for coaching staff
  DebugMode          // Detailed debug information
};

/**
 * @brief Player position data for overlay rendering
 */
struct PlayerPosition {
  QString player_id;
  QString position_type; // "offense", "defense", "special_teams"
  QPointF field_position; // Normalized field coordinates (0.0-1.0)
  double confidence;
  QString jersey_number;
  QString player_name;
  QColor team_color;
  bool is_highlighted;
  QDateTime detection_timestamp;
  
  PlayerPosition() : confidence(0.0), is_highlighted(false) {}
};

/**
 * @brief Formation shape definition for rendering
 */
struct FormationShape {
  QString formation_id;
  FormationType formation_type;
  QList<PlayerPosition> player_positions;
  QList<QLineF> formation_lines; // Lines connecting players
  QPolygonF formation_boundary;
  QPointF center_point;
  double formation_confidence;
  QColor formation_color;
  QString formation_label;
  bool show_connections;
  bool show_boundary;
  QDateTime timestamp;
  
  FormationShape() : formation_type(FormationType::Unknown), formation_confidence(0.0),
                     show_connections(true), show_boundary(false) {}
};

/**
 * @brief Triangle Defense call visualization
 */
struct TriangleCallVisual {
  QString call_id;
  TriangleCall call_type;
  QPointF call_position; // Where to display the call
  QString call_text;
  QColor call_color;
  double call_intensity; // 0.0-1.0 for visual emphasis
  bool animated;
  QDateTime call_timestamp;
  QDateTime expiry_timestamp;
  QJsonObject call_metadata;
  
  TriangleCallVisual() : call_type(TriangleCall::NoCall), call_intensity(1.0), 
                         animated(true) {}
};

/**
 * @brief Overlay rendering statistics
 */
struct OverlayStatistics {
  qint64 frames_rendered;
  qint64 formations_displayed;
  qint64 calls_displayed;
  double average_render_time_ms;
  double current_fps;
  QDateTime last_render_time;
  int active_animations;
  qint64 memory_usage_bytes;
  
  OverlayStatistics() : frames_rendered(0), formations_displayed(0), calls_displayed(0),
                        average_render_time_ms(0.0), current_fps(0.0), active_animations(0),
                        memory_usage_bytes(0) {}
};

/**
 * @brief Formation overlay graphics item
 */
class FormationOverlayItem : public QGraphicsItem
{
public:
  explicit FormationOverlayItem(const FormationShape& formation, QGraphicsItem* parent = nullptr);

  // QGraphicsItem interface
  QRectF boundingRect() const override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

  void updateFormation(const FormationShape& formation);
  void setHighlighted(bool highlighted);
  void startFadeInAnimation();
  void startFadeOutAnimation();
  void setOpacityLevel(double opacity);

protected:
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
  void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;

private:
  void updateVisuals();
  void renderPlayers(QPainter* painter);
  void renderConnections(QPainter* painter);
  void renderBoundary(QPainter* painter);
  void renderLabel(QPainter* painter);

  FormationShape formation_;
  bool is_highlighted_;
  bool is_hovered_;
  QPropertyAnimation* opacity_animation_;
  QElapsedTimer render_timer_;
};

/**
 * @brief Triangle Defense call graphics item
 */
class TriangleCallItem : public QGraphicsItem
{
public:
  explicit TriangleCallItem(const TriangleCallVisual& call, QGraphicsItem* parent = nullptr);

  // QGraphicsItem interface
  QRectF boundingRect() const override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

  void updateCall(const TriangleCallVisual& call);
  void startPulseAnimation();
  void stopPulseAnimation();
  void setIntensity(double intensity);

protected:
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;

private:
  void updateVisuals();
  void renderCallShape(QPainter* painter);
  void renderCallText(QPainter* painter);
  void renderCallIndicators(QPainter* painter);

  TriangleCallVisual call_;
  QPropertyAnimation* pulse_animation_;
  QPropertyAnimation* intensity_animation_;
  double current_intensity_;
  bool is_pulsing_;
};

/**
 * @brief Field overlay for coordinate system
 */
class FieldOverlayItem : public QGraphicsItem
{
public:
  explicit FieldOverlayItem(QGraphicsItem* parent = nullptr);

  // QGraphicsItem interface
  QRectF boundingRect() const override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

  void setFieldDimensions(const QSizeF& field_size);
  void setHashMarks(bool visible);
  void setYardLines(bool visible);
  void setEndZones(bool visible);
  void setFieldZone(const QString& zone); // "Red Zone", "Midfield", etc.

private:
  void renderField(QPainter* painter);
  void renderHashMarks(QPainter* painter);
  void renderYardLines(QPainter* painter);
  void renderEndZones(QPainter* painter);
  void renderZoneHighlight(QPainter* painter);

  QSizeF field_dimensions_;
  bool show_hash_marks_;
  bool show_yard_lines_;
  bool show_end_zones_;
  QString current_field_zone_;
  QColor field_color_;
  QColor line_color_;
};

/**
 * @brief Main formation overlay rendering system
 */
class FormationOverlay : public QWidget
{
  Q_OBJECT

public:
  explicit FormationOverlay(QObject* parent = nullptr);
  virtual ~FormationOverlay();

  /**
   * @brief Initialize overlay system
   */
  bool Initialize();

  /**
   * @brief Shutdown overlay system
   */
  void Shutdown();

  /**
   * @brief Connect to sequence viewer for video overlay
   */
  void ConnectSequenceViewer(SequenceViewerPanel* viewer_panel);

  /**
   * @brief Set overlay rendering mode
   */
  void SetOverlayMode(OverlayMode mode);

  /**
   * @brief Enable/disable real-time mode
   */
  void SetRealTimeMode(bool enabled);

  /**
   * @brief Update current video position
   */
  void UpdateVideoPosition(qint64 timestamp);

  /**
   * @brief Add formation to overlay
   */
  void AddFormation(const FormationData& formation);

  /**
   * @brief Update existing formation
   */
  void UpdateFormation(const FormationData& formation);

  /**
   * @brief Remove formation from overlay
   */
  void RemoveFormation(const QString& formation_id);

  /**
   * @brief Add Triangle Defense call visualization
   */
  void AddTriangleCall(TriangleCall call, const FormationData& formation);

  /**
   * @brief Clear all overlay elements
   */
  void ClearOverlay();

  /**
   * @brief Set field overlay properties
   */
  void SetFieldOverlay(bool enabled, const QSizeF& field_size = QSizeF(120, 53.3));

  /**
   * @brief Set player visibility options
   */
  void SetPlayerVisibility(bool show_numbers, bool show_names, bool show_positions);

  /**
   * @brief Set formation visualization options
   */
  void SetFormationVisibility(bool show_connections, bool show_boundaries, bool show_labels);

  /**
   * @brief Set animation preferences
   */
  void SetAnimationPreferences(bool enable_fade_in, bool enable_pulse, int animation_duration_ms = 500);

  /**
   * @brief Get overlay rendering statistics
   */
  OverlayStatistics GetOverlayStatistics() const;

  /**
   * @brief Export overlay configuration
   */
  QJsonObject ExportOverlayConfig() const;

  /**
   * @brief Import overlay configuration
   */
  bool ImportOverlayConfig(const QJsonObject& config);

  /**
   * @brief Take screenshot of current overlay
   */
  QPixmap CaptureOverlay() const;

  /**
   * @brief Set overlay opacity
   */
  void SetOverlayOpacity(double opacity);

  /**
   * @brief Get current overlay mode
   */
  OverlayMode GetOverlayMode() const { return overlay_mode_; }

public slots:
  /**
   * @brief Handle formation events from Triangle Defense sync
   */
  void OnFormationDetected(const FormationData& formation);
  void OnFormationUpdated(const FormationData& formation);
  void OnFormationDeleted(const QString& formation_id);

  /**
   * @brief Handle Triangle Defense call events
   */
  void OnTriangleCallRecommended(TriangleCall call, const FormationData& formation);
  void OnDefensiveShiftSuggested(const QString& shift_type, const QJsonObject& context);

  /**
   * @brief Handle M.E.L. pipeline events
   */
  void OnMELResultsUpdated(const QString& formation_id, const MELResult& results);

  /**
   * @brief Handle video playback events
   */
  void OnVideoPlaybackChanged(bool playing);
  void OnVideoPositionChanged(qint64 position);
  void OnVideoSizeChanged(const QSize& video_size);

  /**
   * @brief Handle user interaction
   */
  void OnFormationClicked(const QString& formation_id);
  void OnCallClicked(const QString& call_id);
  void OnOverlayRightClicked(const QPoint& position);

signals:
  /**
   * @brief Overlay interaction signals
   */
  void FormationSelected(const QString& formation_id);
  void CallSelected(const QString& call_id);
  void OverlayContextMenuRequested(const QPoint& position);

  /**
   * @brief Overlay state signals
   */
  void OverlayModeChanged(OverlayMode mode);
  void RenderingStatisticsUpdated(const OverlayStatistics& stats);
  void OverlayError(const QString& error);

private slots:
  void OnRenderTimer();
  void OnAnimationTimer();
  void OnCleanupTimer();
  void OnStatisticsTimer();
  void UpdateOverlayGeometry();
  void ProcessRenderQueue();

protected:
  void paintEvent(QPaintEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;

private:
  void SetupGraphicsView();
  void SetupTimers();
  void SetupAnimations();
  void ConnectSignals();
  
  void RenderOverlay();
  void UpdateFormationVisuals();
  void UpdateCallVisuals();
  void UpdateFieldOverlay();
  void OptimizeRendering();
  
  FormationShape ConvertFormationData(const FormationData& formation);
  TriangleCallVisual CreateCallVisual(TriangleCall call, const FormationData& formation);
  QPointF ConvertFieldCoordinates(const QPointF& field_pos, const QSizeF& video_size);
  QPointF ConvertVideoCoordinates(const QPointF& video_pos, const QSizeF& video_size);
  
  void CleanupExpiredElements();
  void UpdateRenderingStatistics();
  void HandleRenderingError(const QString& error);
  void ApplyRenderingOptimizations();

  // Core components
  SequenceViewerPanel* sequence_viewer_;
  QGraphicsView* graphics_view_;
  QGraphicsScene* graphics_scene_;
  FieldOverlayItem* field_overlay_;
  
  // Overlay state
  bool is_initialized_;
  OverlayMode overlay_mode_;
  bool real_time_mode_;
  bool overlay_enabled_;
  qint64 current_video_position_;
  QSize current_video_size_;
  bool video_playing_;
  
  // Formation and call management
  mutable QMutex overlay_mutex_;
  QMap<QString, FormationOverlayItem*> formation_items_;
  QMap<QString, TriangleCallItem*> call_items_;
  QQueue<FormationData> formation_queue_;
  QQueue<TriangleCallVisual> call_queue_;
  
  // Visual configuration
  bool show_field_overlay_;
  bool show_player_numbers_;
  bool show_player_names_;
  bool show_player_positions_;
  bool show_formation_connections_;
  bool show_formation_boundaries_;
  bool show_formation_labels_;
  double overlay_opacity_;
  
  // Animation system
  bool enable_fade_in_animations_;
  bool enable_pulse_animations_;
  int animation_duration_ms_;
  QParallelAnimationGroup* animation_group_;
  QTimer* animation_timer_;
  
  // Rendering optimization
  QTimer* render_timer_;
  QTimer* cleanup_timer_;
  QTimer* statistics_timer_;
  QElapsedTimer frame_timer_;
  bool rendering_optimizations_enabled_;
  int max_rendered_formations_;
  int max_rendered_calls_;
  
  // Performance monitoring
  mutable QMutex stats_mutex_;
  OverlayStatistics overlay_statistics_;
  QQueue<double> render_time_history_;
  QQueue<double> fps_history_;
  int performance_history_size_;
  
  // Configuration
  QJsonObject overlay_configuration_;
  QString overlay_style_sheet_;
  bool debug_rendering_enabled_;
};

/**
 * @brief Utility functions for formation overlay rendering
 */
namespace FormationOverlayUtils {

/**
 * @brief Convert formation type to display color
 */
QColor GetFormationTypeColor(FormationType type, double confidence = 1.0);

/**
 * @brief Convert Triangle call to display color
 */
QColor GetTriangleCallColor(TriangleCall call, double intensity = 1.0);

/**
 * @brief Calculate formation center point
 */
QPointF CalculateFormationCenter(const QList<PlayerPosition>& players);

/**
 * @brief Generate formation boundary polygon
 */
QPolygonF GenerateFormationBoundary(const QList<PlayerPosition>& players);

/**
 * @brief Calculate optimal text size for overlay
 */
int CalculateOptimalTextSize(const QSizeF& overlay_size, const QString& text);

/**
 * @brief Generate formation connection lines
 */
QList<QLineF> GenerateFormationConnections(const QList<PlayerPosition>& players, FormationType type);

/**
 * @brief Validate player position coordinates
 */
bool ValidatePlayerPosition(const PlayerPosition& position);

/**
 * @brief Convert field zone to highlight color
 */
QColor GetFieldZoneColor(const QString& zone);

/**
 * @brief Calculate rendering performance score
 */
double CalculateRenderingPerformance(const OverlayStatistics& stats);

/**
 * @brief Optimize formation data for rendering
 */
FormationShape OptimizeFormationForRendering(const FormationData& formation, const QSizeF& target_size);

} // namespace FormationOverlayUtils

} // namespace olive

Q_DECLARE_METATYPE(olive::OverlayMode)
Q_DECLARE_METATYPE(olive::PlayerPosition)
Q_DECLARE_METATYPE(olive::FormationShape)
Q_DECLARE_METATYPE(olive::TriangleCallVisual)
Q_DECLARE_METATYPE(olive::OverlayStatistics)

#endif // FORMATIONOVERLAY_H
