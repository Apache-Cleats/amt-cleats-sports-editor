/***
  Apache-Cleats Sports Editor
  Copyright (C) 2024 AnalyzeMyTeam

  Formation Overlay Rendering System Implementation
  Real-time Triangle Defense formation visualization on video content
***/

#include "formation_overlay.h"

#include <QDebug>
#include <QOpenGLContext>
#include <QMatrix4x4>
#include <QVector3D>
#include <QVector2D>
#include <QApplication>
#include <QDir>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QtMath>
#include <QElapsedTimer>
#include <QOpenGLFramebufferObject>

#include "panel/sequenceviewer/sequenceviewer.h"
#include "video_timeline_sync.h"

namespace olive {

FormationOverlay::FormationOverlay(QWidget* parent)
  : QOpenGLWidget(parent)
  , triangle_defense_sync_(nullptr)
  , timeline_sync_(nullptr)
  , viewer_panel_(nullptr)
  , render_mode_(OverlayRenderMode::DetailedFormation)
  , real_time_mode_(false)
  , is_initialized_(false)
  , overlay_opacity_(0.8)
  , animations_enabled_(true)
  , current_video_timestamp_(0)
  , formation_shader_(nullptr)
  , triangle_shader_(nullptr)
  , text_shader_(nullptr)
  , vertex_buffer_(nullptr)
  , index_buffer_(nullptr)
  , vertex_array_(nullptr)
  , field_texture_(nullptr)
  , player_icon_texture_(nullptr)
  , fade_animation_(nullptr)
  , animation_group_(nullptr)
  , animation_timer_(nullptr)
  , render_timer_(nullptr)
  , statistics_timer_(nullptr)
  , cleanup_timer_(nullptr)
  , field_template_name_("american_football")
  , font_metrics_(nullptr)
  , last_mouse_position_(0, 0)
  , mouse_pressed_(false)
  , player_marker_size_(8.0)
  , formation_line_width_(2.0)
  , triangle_line_width_(3.0)
  , show_player_labels_(true)
  , show_formation_labels_(true)
  , show_confidence_indicators_(true)
  , gpu_acceleration_enabled_(true)
  , level_of_detail_enabled_(true)
  , max_visible_formations_(50)
  , element_retention_time_ms_(300000) // 5 minutes
  , auto_cleanup_enabled_(true)
  , max_render_history_(100)
{
  qInfo() << "Initializing Formation Overlay";
  
  // Initialize formation colors
  formation_colors_[FormationType::Larry] = QColor(50, 150, 255);     // Blue
  formation_colors_[FormationType::Linda] = QColor(255, 100, 150);    // Pink
  formation_colors_[FormationType::Rita] = QColor(150, 255, 100);     // Light Green
  formation_colors_[FormationType::Ricky] = QColor(255, 150, 50);     // Orange
  formation_colors_[FormationType::Randy] = QColor(200, 100, 255);    // Purple
  formation_colors_[FormationType::Pat] = QColor(150, 150, 150);      // Gray
  formation_colors_[FormationType::Unknown] = QColor(100, 100, 100);  // Dark Gray
  
  // Initialize Triangle Defense colors
  triangle_colors_[TriangleCall::StrongSide] = QColor(255, 50, 50);    // Red
  triangle_colors_[TriangleCall::WeakSide] = QColor(50, 255, 50);      // Green
  triangle_colors_[TriangleCall::MiddleHash] = QColor(255, 255, 50);   // Yellow
  triangle_colors_[TriangleCall::LeftHash] = QColor(50, 50, 255);      // Blue
  triangle_colors_[TriangleCall::RightHash] = QColor(255, 50, 255);    // Magenta
  triangle_colors_[TriangleCall::RedZone] = QColor(255, 100, 0);       // Dark Orange
  triangle_colors_[TriangleCall::GoalLine] = QColor(150, 0, 0);        // Dark Red
  triangle_colors_[TriangleCall::NoCall] = QColor(128, 128, 128);      // Gray
  
  // Setup font
  overlay_font_ = QFont("Arial", 10, QFont::Bold);
  font_metrics_ = new QFontMetrics(overlay_font_);
  
  // Setup field template
  field_dimensions_ = QSizeF(120.0, 53.3); // American football field yards
  field_bounds_ = QRectF(-60.0, -26.65, 120.0, 53.3);
  
  // Enable OpenGL features
  QSurfaceFormat format;
  format.setVersion(3, 3);
  format.setProfile(QSurfaceFormat::CoreProfile);
  format.setSamples(4); // Anti-aliasing
  format.setDepthBufferSize(24);
  setFormat(format);
  
  // Setup timers
  render_timer_ = new QTimer(this);
  render_timer_->setInterval(16); // ~60 FPS
  connect(render_timer_, &QTimer::timeout, this, &FormationOverlay::OnRenderTimer);
  
  animation_timer_ = new QTimer(this);
  animation_timer_->setInterval(16); // ~60 FPS
  connect(animation_timer_, &QTimer::timeout, this, &FormationOverlay::OnAnimationTimer);
  
  statistics_timer_ = new QTimer(this);
  statistics_timer_->setInterval(1000); // 1 second
  connect(statistics_timer_, &QTimer::timeout, this, &FormationOverlay::OnStatisticsTimer);
  
  cleanup_timer_ = new QTimer(this);
  cleanup_timer_->setInterval(60000); // 1 minute
  connect(cleanup_timer_, &QTimer::timeout, this, &FormationOverlay::CleanupOldElements);
  
  // Setup animation group
  animation_group_ = new QParallelAnimationGroup(this);
  fade_animation_ = new QPropertyAnimation(this, "overlay_opacity");
  
  qInfo() << "Formation Overlay initialized";
}

FormationOverlay::~FormationOverlay()
{
  Shutdown();
  delete font_metrics_;
}

bool FormationOverlay::Initialize(TriangleDefenseSync* triangle_sync, VideoTimelineSync* timeline_sync)
{
  if (is_initialized_) {
    qWarning() << "Formation Overlay already initialized";
    return true;
  }

  triangle_defense_sync_ = triangle_sync;
  timeline_sync_ = timeline_sync;

  if (triangle_defense_sync_) {
    connect(triangle_defense_sync_, &TriangleDefenseSync::FormationDetected,
            this, &FormationOverlay::OnFormationDetected);
    connect(triangle_defense_sync_, &TriangleDefenseSync::FormationUpdated,
            this, &FormationOverlay::OnFormationUpdated);
    connect(triangle_defense_sync_, &TriangleDefenseSync::TriangleCallRecommended,
            this, &FormationOverlay::OnTriangleCallRecommended);
    connect(triangle_defense_sync_, &TriangleDefenseSync::MELResultsUpdated,
            this, &FormationOverlay::OnMELResultsUpdated);

    qInfo() << "Triangle Defense sync connected to Formation Overlay";
  }

  // Load field template
  LoadFieldTemplate();

  // Start statistics timer
  statistics_timer_->start();
  cleanup_timer_->start();

  is_initialized_ = true;
  qInfo() << "Formation Overlay initialization completed";

  return true;
}

void FormationOverlay::Shutdown()
{
  if (!is_initialized_) {
    return;
  }

  qInfo() << "Shutting down Formation Overlay";

  // Stop real-time mode
  if (real_time_mode_) {
    SetRealTimeMode(false);
  }

  // Stop timers
  if (render_timer_ && render_timer_->isActive()) {
    render_timer_->stop();
  }
  if (animation_timer_ && animation_timer_->isActive()) {
    animation_timer_->stop();
  }
  if (statistics_timer_ && statistics_timer_->isActive()) {
    statistics_timer_->stop();
  }
  if (cleanup_timer_ && cleanup_timer_->isActive()) {
    cleanup_timer_->stop();
  }

  // Stop animations
  if (animation_group_) {
    animation_group_->stop();
  }
  if (fade_animation_) {
    fade_animation_->stop();
  }

  // Clear overlay data
  ClearOverlay();

  // Clean up OpenGL resources
  makeCurrent();
  
  delete formation_shader_;
  delete triangle_shader_;
  delete text_shader_;
  delete vertex_buffer_;
  delete index_buffer_;
  delete vertex_array_;
  delete field_texture_;
  delete player_icon_texture_;
  
  formation_shader_ = nullptr;
  triangle_shader_ = nullptr;
  text_shader_ = nullptr;
  vertex_buffer_ = nullptr;
  index_buffer_ = nullptr;
  vertex_array_ = nullptr;
  field_texture_ = nullptr;
  player_icon_texture_ = nullptr;

  doneCurrent();

  is_initialized_ = false;
  qInfo() << "Formation Overlay shutdown complete";
}

void FormationOverlay::ConnectSequenceViewer(SequenceViewerPanel* viewer_panel)
{
  if (viewer_panel_) {
    disconnect(viewer_panel_, nullptr, this, nullptr);
  }

  viewer_panel_ = viewer_panel;

  if (viewer_panel_) {
    // Connect sequence viewer events
    connect(viewer_panel_, &SequenceViewerPanel::PlaybackStarted,
            this, &FormationOverlay::OnVideoPlaybackStarted);
    connect(viewer_panel_, &SequenceViewerPanel::PlaybackStopped,
            this, &FormationOverlay::OnVideoPlaybackStopped);
    connect(viewer_panel_, &SequenceViewerPanel::PositionChanged,
            this, &FormationOverlay::OnVideoPositionChanged);
    connect(viewer_panel_, &SequenceViewerPanel::SeekPerformed,
            this, &FormationOverlay::OnVideoSeekPerformed);

    qInfo() << "Sequence viewer connected to Formation Overlay";
  }
}

void FormationOverlay::SetRenderMode(OverlayRenderMode mode)
{
  if (render_mode_ != mode) {
    render_mode_ = mode;
    
    // Adjust rendering parameters based on mode
    switch (mode) {
      case OverlayRenderMode::Disabled:
        setVisible(false);
        break;
        
      case OverlayRenderMode::MinimalMarkers:
        setVisible(true);
        player_marker_size_ = 4.0;
        show_player_labels_ = false;
        show_formation_labels_ = false;
        show_confidence_indicators_ = false;
        break;
        
      case OverlayRenderMode::BasicFormation:
        setVisible(true);
        player_marker_size_ = 6.0;
        show_player_labels_ = false;
        show_formation_labels_ = true;
        show_confidence_indicators_ = false;
        break;
        
      case OverlayRenderMode::DetailedFormation:
        setVisible(true);
        player_marker_size_ = 8.0;
        show_player_labels_ = true;
        show_formation_labels_ = true;
        show_confidence_indicators_ = true;
        break;
        
      case OverlayRenderMode::TriangleDefense:
        setVisible(true);
        player_marker_size_ = 8.0;
        show_player_labels_ = true;
        show_formation_labels_ = true;
        show_confidence_indicators_ = true;
        break;
        
      case OverlayRenderMode::MELVisualization:
        setVisible(true);
        player_marker_size_ = 6.0;
        show_player_labels_ = false;
        show_formation_labels_ = true;
        show_confidence_indicators_ = true;
        break;
        
      case OverlayRenderMode::ComprehensiveView:
        setVisible(true);
        player_marker_size_ = 10.0;
        show_player_labels_ = true;
        show_formation_labels_ = true;
        show_confidence_indicators_ = true;
        break;
    }
    
    emit RenderModeChanged(mode);
    update(); // Trigger repaint
    
    qInfo() << "Overlay render mode changed to:" << static_cast<int>(mode);
  }
}

void FormationOverlay::SetRealTimeMode(bool enabled)
{
  if (real_time_mode_ != enabled) {
    real_time_mode_ = enabled;

    if (enabled) {
      render_timer_->start();
      if (animations_enabled_) {
        animation_timer_->start();
      }
      qInfo() << "Real-time overlay mode enabled";
    } else {
      render_timer_->stop();
      animation_timer_->stop();
      qInfo() << "Real-time overlay mode disabled";
    }
  }
}

void FormationOverlay::UpdateVideoPosition(qint64 timestamp)
{
  current_video_timestamp_ = timestamp;
  
  if (real_time_mode_) {
    // Update overlay elements based on current timestamp
    UpdateFormationShapes();
    UpdatePlayerPositions();
    UpdateTriangleVisualizations();
    UpdateMELVisualizations();
    
    update(); // Trigger repaint
  }
}

void FormationOverlay::AddFormation(const FormationData& formation)
{
  QMutexLocker locker(&overlay_mutex_);
  
  FormationShape formation_shape;
  formation_shape.formation_id = formation.formation_id;
  formation_shape.formation_type = formation.type;
  formation_shape.formation_color = GetFormationColor(formation.type, formation.confidence);
  formation_shape.formation_label = QString("Formation %1").arg(static_cast<int>(formation.type));
  formation_shape.confidence = formation.confidence;
  formation_shape.show_connections = true;
  formation_shape.animated = formation.confidence > 0.8;
  formation_shape.timestamp = formation.video_timestamp;
  
  // Extract player positions from formation data
  if (formation.player_positions.contains("players")) {
    QJsonArray players_array = formation.player_positions["players"].toArray();
    
    for (const QJsonValue& player_value : players_array) {
      QJsonObject player_obj = player_value.toObject();
      
      PlayerPosition player;
      player.player_id = player_obj["id"].toString();
      player.position_label = player_obj["position"].toString();
      player.field_position = QPointF(player_obj["x"].toDouble(), player_obj["y"].toDouble());
      player.screen_position = FieldToScreen(player.field_position);
      player.marker_color = formation_shape.formation_color;
      player.confidence = formation.confidence;
      player.timestamp = formation.video_timestamp;
      
      formation_shape.players.append(player);
      players_[player.player_id] = player;
    }
  }
  
  // Calculate formation outline and center
  CalculateFormationOutline(formation_shape);
  formation_shape.center_point = FormationOverlayUtils::CalculateFormationCentroid(formation_shape.players);
  
  formations_[formation.formation_id] = formation_shape;
  
  // Start animation if enabled
  if (animations_enabled_ && formation_shape.animated) {
    StartAnimation(formation.formation_id, "fade", 1.0, 1000);
  }
  
  // Update statistics
  {
    QMutexLocker stats_locker(&stats_mutex_);
    overlay_statistics_.formations_displayed++;
  }
  
  qDebug() << "Formation added to overlay:" << formation.formation_id 
           << "type:" << static_cast<int>(formation.type)
           << "players:" << formation_shape.players.size();
}

void FormationOverlay::UpdateFormation(const FormationData& formation)
{
  QMutexLocker locker(&overlay_mutex_);
  
  if (formations_.contains(formation.formation_id)) {
    FormationShape& formation_shape = formations_[formation.formation_id];
    
    // Update formation properties
    formation_shape.confidence = formation.confidence;
    formation_shape.formation_color = GetFormationColor(formation.type, formation.confidence);
    formation_shape.timestamp = formation.video_timestamp;
    
    // Update player positions if provided
    if (formation.player_positions.contains("players")) {
      QJsonArray players_array = formation.player_positions["players"].toArray();
      
      // Clear existing players
      formation_shape.players.clear();
      
      for (const QJsonValue& player_value : players_array) {
        QJsonObject player_obj = player_value.toObject();
        
        PlayerPosition player;
        player.player_id = player_obj["id"].toString();
        player.position_label = player_obj["position"].toString();
        player.field_position = QPointF(player_obj["x"].toDouble(), player_obj["y"].toDouble());
        player.screen_position = FieldToScreen(player.field_position);
        player.marker_color = formation_shape.formation_color;
        player.confidence = formation.confidence;
        player.timestamp = formation.video_timestamp;
        
        formation_shape.players.append(player);
        players_[player.player_id] = player;
      }
      
      // Recalculate formation outline and center
      CalculateFormationOutline(formation_shape);
      formation_shape.center_point = FormationOverlayUtils::CalculateFormationCentroid(formation_shape.players);
    }
    
    qDebug() << "Formation updated in overlay:" << formation.formation_id;
  }
}

void FormationOverlay::RemoveFormation(const QString& formation_id)
{
  QMutexLocker locker(&overlay_mutex_);
  
  if (formations_.contains(formation_id)) {
    // Remove associated players
    const FormationShape& formation = formations_[formation_id];
    for (const PlayerPosition& player : formation.players) {
      players_.remove(player.player_id);
    }
    
    formations_.remove(formation_id);
    
    // Stop any active animations
    StopAnimation(formation_id);
    
    qDebug() << "Formation removed from overlay:" << formation_id;
  }
}

void FormationOverlay::AddTriangleVisualization(TriangleCall call, const FormationData& formation)
{
  QMutexLocker locker(&overlay_mutex_);
  
  TriangleVisualization triangle_viz;
  triangle_viz.triangle_id = QString("triangle_%1_%2").arg(formation.formation_id).arg(static_cast<int>(call));
  triangle_viz.call_type = call;
  triangle_viz.triangle_color = GetTriangleColor(call);
  triangle_viz.arrow_color = triangle_viz.triangle_color.darker(150);
  triangle_viz.call_label = QString("Triangle: %1").arg(static_cast<int>(call));
  triangle_viz.show_arrows = true;
  triangle_viz.show_zones = true;
  triangle_viz.urgency_level = formation.confidence;
  triangle_viz.timestamp = formation.video_timestamp;
  
  // Calculate triangle points based on formation and call type
  if (formations_.contains(formation.formation_id)) {
    const FormationShape& formation_shape = formations_[formation.formation_id];
    
    // Generate triangle points based on call type and player positions
    switch (call) {
      case TriangleCall::StrongSide:
        // Create triangle focusing on strong side
        if (formation_shape.players.size() >= 3) {
          triangle_viz.triangle_points.append(formation_shape.players[0].field_position);
          triangle_viz.triangle_points.append(formation_shape.players[1].field_position);
          triangle_viz.triangle_points.append(formation_shape.center_point);
        }
        break;
        
      case TriangleCall::WeakSide:
        // Create triangle focusing on weak side
        if (formation_shape.players.size() >= 3) {
          int weak_side_start = formation_shape.players.size() / 2;
          triangle_viz.triangle_points.append(formation_shape.players[weak_side_start].field_position);
          triangle_viz.triangle_points.append(formation_shape.players[weak_side_start + 1].field_position);
          triangle_viz.triangle_points.append(formation_shape.center_point);
        }
        break;
        
      case TriangleCall::MiddleHash:
        // Create triangle focusing on middle
        triangle_viz.triangle_points.append(QPointF(0, -10));
        triangle_viz.triangle_points.append(QPointF(-5, 5));
        triangle_viz.triangle_points.append(QPointF(5, 5));
        break;
        
      default:
        // Default triangle around formation center
        QPointF center = formation_shape.center_point;
        triangle_viz.triangle_points.append(center + QPointF(0, -10));
        triangle_viz.triangle_points.append(center + QPointF(-8, 5));
        triangle_viz.triangle_points.append(center + QPointF(8, 5));
        break;
    }
    
    triangle_viz.call_position = formation_shape.center_point;
  }
  
  triangles_[triangle_viz.triangle_id] = triangle_viz;
  
  // Start animation if enabled
  if (animations_enabled_) {
    StartAnimation(triangle_viz.triangle_id, "pulse", 1.2, 1500);
  }
  
  // Update statistics
  {
    QMutexLocker stats_locker(&stats_mutex_);
    overlay_statistics_.triangles_rendered++;
  }
  
  qDebug() << "Triangle visualization added:" << triangle_viz.triangle_id 
           << "call:" << static_cast<int>(call);
}

void FormationOverlay::AddMELVisualization(const QString& formation_id, const MELResult& results)
{
  QMutexLocker locker(&overlay_mutex_);
  
  MELVisualization mel_viz;
  mel_viz.mel_id = QString("mel_%1").arg(formation_id);
  mel_viz.mel_results = results;
  mel_viz.show_detailed_scores = true;
  mel_viz.show_progress_bars = true;
  mel_viz.animated = results.combined_score > 85.0;
  mel_viz.timestamp = results.processing_timestamp;
  
  // Position M.E.L. visualization
  if (formations_.contains(formation_id)) {
    const FormationShape& formation = formations_[formation_id];
    mel_viz.display_position = formation.center_point + QPointF(15, -15);
  } else {
    mel_viz.display_position = QPointF(10, 10); // Top-left corner
  }
  
  mel_viz.display_size = QSizeF(120, 80);
  mel_viz.background_color = QColor(0, 0, 0, 180); // Semi-transparent black
  mel_viz.text_color = QColor(255, 255, 255);
  
  mel_visualizations_[mel_viz.mel_id] = mel_viz;
  
  // Start animation if enabled
  if (animations_enabled_ && mel_viz.animated) {
    StartAnimation(mel_viz.mel_id, "scale", 1.1, 1000);
  }
  
  // Update statistics
  {
    QMutexLocker stats_locker(&stats_mutex_);
    overlay_statistics_.mel_visualizations_shown++;
  }
  
  qDebug() << "M.E.L. visualization added:" << mel_viz.mel_id 
           << "score:" << results.combined_score;
}

void FormationOverlay::ClearOverlay()
{
  QMutexLocker locker(&overlay_mutex_);
  
  formations_.clear();
  triangles_.clear();
  mel_visualizations_.clear();
  players_.clear();
  
  // Stop all animations
  active_animations_.clear();
  if (animation_group_) {
    animation_group_->stop();
  }
  
  selected_element_id_.clear();
  hovered_element_id_.clear();
  
  update(); // Trigger repaint
  
  qInfo() << "Formation overlay cleared";
}

void FormationOverlay::SetOverlayOpacity(double opacity)
{
  overlay_opacity_ = qBound(0.0, opacity, 1.0);
  
  if (fade_animation_) {
    fade_animation_->stop();
    fade_animation_->setStartValue(overlay_opacity_);
    fade_animation_->setEndValue(opacity);
    fade_animation_->setDuration(500);
    fade_animation_->start();
  }
  
  emit OverlayOpacityChanged(overlay_opacity_);
  update();
}

void FormationOverlay::SetAnimationsEnabled(bool enabled)
{
  animations_enabled_ = enabled;
  
  if (!enabled) {
    // Stop all current animations
    active_animations_.clear();
    if (animation_group_) {
      animation_group_->stop();
    }
    if (animation_timer_) {
      animation_timer_->stop();
    }
  } else if (real_time_mode_) {
    // Restart animation timer if in real-time mode
    if (animation_timer_) {
      animation_timer_->start();
    }
  }
  
  qInfo() << "Overlay animations" << (enabled ? "enabled" : "disabled");
}

void FormationOverlay::SetFieldTemplate(const QString& template_name)
{
  if (field_template_name_ != template_name) {
    field_template_name_ = template_name;
    LoadFieldTemplate();
    
    // Recalculate all screen positions
    UpdateFormationShapes();
    UpdatePlayerPositions();
    
    update(); // Trigger repaint
    
    qInfo() << "Field template changed to:" << template_name;
  }
}

OverlayStatistics FormationOverlay::GetOverlayStatistics() const
{
  QMutexLocker locker(&stats_mutex_);
  return overlay_statistics_;
}

QJsonObject FormationOverlay::ExportOverlayConfig() const
{
  QJsonObject config;
  
  config["render_mode"] = static_cast<int>(render_mode_);
  config["overlay_opacity"] = overlay_opacity_;
  config["animations_enabled"] = animations_enabled_;
  config["field_template"] = field_template_name_;
  config["player_marker_size"] = player_marker_size_;
  config["formation_line_width"] = formation_line_width_;
  config["triangle_line_width"] = triangle_line_width_;
  config["show_player_labels"] = show_player_labels_;
  config["show_formation_labels"] = show_formation_labels_;
  config["show_confidence_indicators"] = show_confidence_indicators_;
  
  // Export color configurations
  QJsonObject formation_colors;
  for (auto it = formation_colors_.constBegin(); it != formation_colors_.constEnd(); ++it) {
    formation_colors[QString::number(static_cast<int>(it.key()))] = it.value().name();
  }
  config["formation_colors"] = formation_colors;
  
  QJsonObject triangle_colors;
  for (auto it = triangle_colors_.constBegin(); it != triangle_colors_.constEnd(); ++it) {
    triangle_colors[QString::number(static_cast<int>(it.key()))] = it.value().name();
  }
  config["triangle_colors"] = triangle_colors;
  
  return config;
}

bool FormationOverlay::ImportOverlayConfig(const QJsonObject& config)
{
  if (config.contains("render_mode")) {
    SetRenderMode(static_cast<OverlayRenderMode>(config["render_mode"].toInt()));
  }
  
  if (config.contains("overlay_opacity")) {
    SetOverlayOpacity(config["overlay_opacity"].toDouble());
  }
  
  if (config.contains("animations_enabled")) {
    SetAnimationsEnabled(config["animations_enabled"].toBool());
  }
  
  if (config.contains("field_template")) {
    SetFieldTemplate(config["field_template"].toString());
  }
  
  if (config.contains("player_marker_size")) {
    player_marker_size_ = config["player_marker_size"].toDouble();
  }
  
  if (config.contains("formation_line_width")) {
    formation_line_width_ = config["formation_line_width"].toDouble();
  }
  
  if (config.contains("triangle_line_width")) {
    triangle_line_width_ = config["triangle_line_width"].toDouble();
  }
  
  if (config.contains("show_player_labels")) {
    show_player_labels_ = config["show_player_labels"].toBool();
  }
  
  if (config.contains("show_formation_labels")) {
    show_formation_labels_ = config["show_formation_labels"].toBool();
  }
  
  if (config.contains("show_confidence_indicators")) {
    show_confidence_indicators_ = config["show_confidence_indicators"].toBool();
  }
  
  // Import color configurations
  if (config.contains("formation_colors")) {
    QJsonObject formation_colors = config["formation_colors"].toObject();
    for (auto it = formation_colors.constBegin(); it != formation_colors.constEnd(); ++it) {
      FormationType type = static_cast<FormationType>(it.key().toInt());
      formation_colors_[type] = QColor(it.value().toString());
    }
  }
  
  if (config.contains("triangle_colors")) {
    QJsonObject triangle_colors = config["triangle_colors"].toObject();
    for (auto it = triangle_colors.constBegin(); it != triangle_colors.constEnd(); ++it) {
      TriangleCall call = static_cast<TriangleCall>(it.key().toInt());
      triangle_colors_[call] = QColor(it.value().toString());
    }
  }
  
  update(); // Trigger repaint
  
  qInfo() << "Overlay configuration imported successfully";
  return true;
}

QPixmap FormationOverlay::CaptureOverlay() const
{
  // Capture current OpenGL framebuffer
  return grabFramebuffer();
}

void FormationOverlay::OnFormationDetected(const FormationData& formation)
{
  if (render_mode_ == OverlayRenderMode::Disabled) {
    return;
  }
  
  AddFormation(formation);
}

void FormationOverlay::OnFormationUpdated(const FormationData& formation)
{
  if (render_mode_ == OverlayRenderMode::Disabled) {
    return;
  }
  
  UpdateFormation(formation);
}

void FormationOverlay::OnFormationDeleted(const QString& formation_id)
{
  RemoveFormation(formation_id);
}

void FormationOverlay::OnTriangleCallRecommended(TriangleCall call, const FormationData& formation)
{
  if (render_mode_ == OverlayRenderMode::TriangleDefense ||
      render_mode_ == OverlayRenderMode::ComprehensiveView) {
    AddTriangleVisualization(call, formation);
  }
}

void FormationOverlay::OnMELResultsUpdated(const QString& formation_id, const MELResult& results)
{
  if (render_mode_ == OverlayRenderMode::MELVisualization ||
      render_mode_ == OverlayRenderMode::ComprehensiveView) {
    AddMELVisualization(formation_id, results);
  }
}

void FormationOverlay::OnVideoPlaybackStarted()
{
  qDebug() << "Video playback started - overlay active";
}

void FormationOverlay::OnVideoPlaybackStopped()
{
  qDebug() << "Video playback stopped - overlay paused";
}

void FormationOverlay::OnVideoPositionChanged(qint64 position)
{
  UpdateVideoPosition(position);
}

void FormationOverlay::OnVideoSeekPerformed(qint64 position)
{
  current_video_timestamp_ = position;
  
  // Clear old elements and update for new position
  if (auto_cleanup_enabled_) {
    CleanupOldElements();
  }
  
  UpdateVideoPosition(position);
}

void FormationOverlay::initializeGL()
{
  initializeOpenGLFunctions();
  
  qInfo() << "Initializing OpenGL for Formation Overlay";
  qInfo() << "OpenGL Version:" << reinterpret_cast<const char*>(glGetString(GL_VERSION));
  qInfo() << "OpenGL Vendor:" << reinterpret_cast<const char*>(glGetString(GL_VENDOR));
  qInfo() << "OpenGL Renderer:" << reinterpret_cast<const char*>(glGetString(GL_RENDERER));
  
  SetupOpenGL();
  SetupShaders();
  SetupBuffers();
  SetupTextures();
  
  qInfo() << "OpenGL initialization complete";
}

void FormationOverlay::resizeGL(int width, int height)
{
  glViewport(0, 0, width, height);
  
  // Update field bounds for new aspect ratio
  double aspect_ratio = static_cast<double>(width) / height;
  double field_aspect = field_dimensions_.width() / field_dimensions_.height();
  
  if (aspect_ratio > field_aspect) {
    // Window is wider than field
    double field_height = field_dimensions_.height();
    double field_width = field_height * aspect_ratio;
    field_bounds_ = QRectF(-field_width / 2, -field_height / 2, field_width, field_height);
  } else {
    // Window is taller than field
    double field_width = field_dimensions_.width();
    double field_height = field_width / aspect_ratio;
    field_bounds_ = QRectF(-field_width / 2, -field_height / 2, field_width, field_height);
  }
  
  // Update all screen positions
  UpdateFormationShapes();
  UpdatePlayerPositions();
}

void FormationOverlay::paintGL()
{
  if (render_mode_ == OverlayRenderMode::Disabled) {
    return;
  }
  
  QElapsedTimer render_timer;
  render_timer.start();
  
  // Clear with transparent background
  glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  
  // Enable blending for transparency
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  
  // Set up projection matrix
  QMatrix4x4 projection;
  projection.ortho(field_bounds_);
  
  RenderOverlay();
  
  // Update render statistics
  double render_time = render_timer.nsecsElapsed() / 1000000.0; // Convert to milliseconds
  
  {
    QMutexLocker locker(&stats_mutex_);
    overlay_statistics_.frames_rendered++;
    
    render_time_history_.enqueue(render_time);
    if (render_time_history_.size() > max_render_history_) {
      render_time_history_.dequeue();
    }
    
    // Calculate average render time
    double total_time = 0.0;
    for (double time : render_time_history_) {
      total_time += time;
    }
    overlay_statistics_.average_render_time_ms = total_time / render_time_history_.size();
  }
}

void FormationOverlay::mousePressEvent(QMouseEvent* event)
{
  last_mouse_position_ = event->position();
  mouse_pressed_ = true;
  
  // Convert to field coordinates
  QPointF field_position = ScreenToField(last_mouse_position_);
  
  // Check for clicks on overlay elements
  PlayerPosition* clicked_player = FindPlayerAt(last_mouse_position_);
  if (clicked_player) {
    selected_element_id_ = clicked_player->player_id;
    emit PlayerClicked(clicked_player->player_id, *clicked_player);
    return;
  }
  
  FormationShape* clicked_formation = FindFormationAt(last_mouse_position_);
  if (clicked_formation) {
    selected_element_id_ = clicked_formation->formation_id;
    emit FormationSelected(clicked_formation->formation_id);
    return;
  }
  
  TriangleVisualization* clicked_triangle = FindTriangleAt(last_mouse_position_);
  if (clicked_triangle) {
    selected_element_id_ = clicked_triangle->triangle_id;
    emit TriangleCallClicked(clicked_triangle->call_type, field_position);
    return;
  }
  
  MELVisualization* clicked_mel = FindMELVisualizationAt(last_mouse_position_);
  if (clicked_mel) {
    selected_element_id_ = clicked_mel->mel_id;
    emit MELVisualizationClicked(clicked_mel->mel_id);
    return;
  }
  
  // Clear selection if nothing clicked
  selected_element_id_.clear();
}

void FormationOverlay::mouseReleaseEvent(QMouseEvent* event)
{
  Q_UNUSED(event)
  mouse_pressed_ = false;
}

void FormationOverlay::mouseMoveEvent(QMouseEvent* event)
{
  last_mouse_position_ = event->position();
  
  // Update hovered element
  QString previous_hovered = hovered_element_id_;
  hovered_element_id_.clear();
  
  PlayerPosition* hovered_player = FindPlayerAt(last_mouse_position_);
  if (hovered_player) {
    hovered_element_id_ = hovered_player->player_id;
    setCursor(Qt::PointingHandCursor);
  } else {
    FormationShape* hovered_formation = FindFormationAt(last_mouse_position_);
    if (hovered_formation) {
      hovered_element_id_ = hovered_formation->formation_id;
      setCursor(Qt::PointingHandCursor);
    } else {
      setCursor(Qt::ArrowCursor);
    }
  }
  
  // Update display if hover state changed
  if (hovered_element_id_ != previous_hovered) {
    update();
  }
}

void FormationOverlay::wheelEvent(QWheelEvent* event)
{
  // Handle zoom functionality
  double zoom_factor = 1.0 + (event->angleDelta().y() / 1200.0);
  
  // Scale field bounds
  QPointF center = field_bounds_.center();
  QSizeF new_size = field_bounds_.size() / zoom_factor;
  
  field_bounds_ = QRectF(center - QPointF(new_size.width() / 2, new_size.height() / 2), new_size);
  
  // Update screen positions
  UpdateFormationShapes();
  UpdatePlayerPositions();
  
  update();
}

void FormationOverlay::keyPressEvent(QKeyEvent* event)
{
  switch (event->key()) {
    case Qt::Key_Space:
      // Toggle overlay visibility
      setVisible(!isVisible());
      break;
      
    case Qt::Key_R:
      // Reset view
      resizeGL(width(), height());
      update();
      break;
      
    case Qt::Key_C:
      // Clear overlay
      ClearOverlay();
      break;
      
    default:
      QOpenGLWidget::keyPressEvent(event);
      break;
  }
}

void FormationOverlay::SetupOpenGL()
{
  // Enable depth testing
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL);
  
  // Enable anti-aliasing
  glEnable(GL_MULTISAMPLE);
  
  // Set line width for smooth lines
  glLineWidth(1.0f);
  
  qDebug() << "OpenGL setup complete";
}

void FormationOverlay::SetupShaders()
{
  // Formation shader for rendering formation shapes
  formation_shader_ = new QOpenGLShaderProgram(this);
  formation_shader_->addShaderFromSourceCode(QOpenGLShader::Vertex, R"(
    #version 330 core
    layout (location = 0) in vec2 position;
    layout (location = 1) in vec4 color;
    
    uniform mat4 projection;
    out vec4 vertex_color;
    
    void main() {
      gl_Position = projection * vec4(position, 0.0, 1.0);
      vertex_color = color;
    }
  )");
  
  formation_shader_->addShaderFromSourceCode(QOpenGLShader::Fragment, R"(
    #version 330 core
    in vec4 vertex_color;
    out vec4 fragment_color;
    
    void main() {
      fragment_color = vertex_color;
    }
  )");
  
  if (!formation_shader_->link()) {
    qCritical() << "Failed to link formation shader:" << formation_shader_->log();
  }
  
  // Triangle shader for Triangle Defense visualization
  triangle_shader_ = new QOpenGLShaderProgram(this);
  triangle_shader_->addShaderFromSourceCode(QOpenGLShader::Vertex, R"(
    #version 330 core
    layout (location = 0) in vec2 position;
    layout (location = 1) in vec4 color;
    
    uniform mat4 projection;
    uniform float time;
    out vec4 vertex_color;
    
    void main() {
      vec2 animated_pos = position;
      // Add subtle animation for triangle visualization
      animated_pos += sin(time) * 0.1;
      
      gl_Position = projection * vec4(animated_pos, 0.0, 1.0);
      vertex_color = color;
    }
  )");
  
  triangle_shader_->addShaderFromSourceCode(QOpenGLShader::Fragment, R"(
    #version 330 core
    in vec4 vertex_color;
    out vec4 fragment_color;
    
    void main() {
      fragment_color = vertex_color;
    }
  )");
  
  if (!triangle_shader_->link()) {
    qCritical() << "Failed to link triangle shader:" << triangle_shader_->log();
  }
  
  qDebug() << "Shaders compiled and linked successfully";
}

void FormationOverlay::SetupBuffers()
{
  // Create vertex array object
  vertex_array_ = new QOpenGLVertexArrayObject(this);
  vertex_array_->create();
  vertex_array_->bind();
  
  // Create vertex buffer
  vertex_buffer_ = new QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
  vertex_buffer_->create();
  vertex_buffer_->bind();
  vertex_buffer_->setUsagePattern(QOpenGLBuffer::DynamicDraw);
  
  // Create index buffer
  index_buffer_ = new QOpenGLBuffer(QOpenGLBuffer::IndexBuffer);
  index_buffer_->create();
  index_buffer_->bind();
  index_buffer_->setUsagePattern(QOpenGLBuffer::DynamicDraw);
  
  vertex_array_->release();
  
  qDebug() << "OpenGL buffers created";
}

void FormationOverlay::SetupTextures()
{
  // Create field texture (placeholder)
  field_texture_ = new QOpenGLTexture(QOpenGLTexture::Target2D);
  field_texture_->setMinificationFilter(QOpenGLTexture::Linear);
  field_texture_->setMagnificationFilter(QOpenGLTexture::Linear);
  
  // Create player icon texture (placeholder)
  player_icon_texture_ = new QOpenGLTexture(QOpenGLTexture::Target2D);
  player_icon_texture_->setMinificationFilter(QOpenGLTexture::Linear);
  player_icon_texture_->setMagnificationFilter(QOpenGLTexture::Linear);
  
  qDebug() << "OpenGL textures created";
}

void FormationOverlay::LoadFieldTemplate()
{
  // Load field template based on template name
  if (field_template_name_ == "american_football") {
    field_dimensions_ = QSizeF(120.0, 53.3); // 120 yards x 53.3 yards
    field_bounds_ = QRectF(-60.0, -26.65, 120.0, 53.3);
  } else if (field_template_name_ == "soccer") {
    field_dimensions_ = QSizeF(110.0, 70.0); // 110m x 70m
    field_bounds_ = QRectF(-55.0, -35.0, 110.0, 70.0);
  } else {
    // Default to American football
    field_dimensions_ = QSizeF(120.0, 53.3);
    field_bounds_ = QRectF(-60.0, -26.65, 120.0, 53.3);
  }
  
  qDebug() << "Field template loaded:" << field_template_name_
           << "dimensions:" << field_dimensions_
           << "bounds:" << field_bounds_;
}

void FormationOverlay::RenderOverlay()
{
  QMutexLocker locker(&overlay_mutex_);
  
  // Set global opacity
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  
  // Render based on current mode
  switch (render_mode_) {
    case OverlayRenderMode::MinimalMarkers:
      RenderPlayerPositions();
      break;
      
    case OverlayRenderMode::BasicFormation:
      RenderPlayerPositions();
      RenderFormations();
      break;
      
    case OverlayRenderMode::DetailedFormation:
      RenderPlayerPositions();
      RenderFormations();
      break;
      
    case OverlayRenderMode::TriangleDefense:
      RenderPlayerPositions();
      RenderFormations();
      RenderTriangleDefense();
      break;
      
    case OverlayRenderMode::MELVisualization:
      RenderPlayerPositions();
      RenderFormations();
      RenderMELVisualizations();
      break;
      
    case OverlayRenderMode::ComprehensiveView:
      RenderPlayerPositions();
      RenderFormations();
      RenderTriangleDefense();
      RenderMELVisualizations();
      break;
      
    default:
      break;
  }
}

void FormationOverlay::RenderFormations()
{
  // Render formation shapes and connections
  for (const FormationShape& formation : formations_) {
    RenderFormationShape(formation);
  }
}

void FormationOverlay::RenderTriangleDefense()
{
  // Render Triangle Defense visualizations
  for (const TriangleVisualization& triangle : triangles_) {
    RenderTriangleVisualization(triangle);
  }
}

void FormationOverlay::RenderMELVisualizations()
{
  // Render M.E.L. score visualizations
  for (const MELVisualization& mel_viz : mel_visualizations_) {
    RenderMELVisualization(mel_viz);
  }
}

void FormationOverlay::RenderPlayerPositions()
{
  // Render player position markers
  for (const PlayerPosition& player : players_) {
    RenderPlayerPosition(player);
  }
}

void FormationOverlay::RenderFormationShape(const FormationShape& formation)
{
  // Render formation outline
  if (!formation.formation_outline.isEmpty()) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(formation.formation_color, formation_line_width_));
    painter.setBrush(QBrush(formation.formation_color, Qt::NoBrush));
    
    // Convert field coordinates to screen coordinates
    QPolygonF screen_outline;
    for (const QPointF& point : formation.formation_outline) {
      screen_outline.append(FieldToScreen(point));
    }
    
    painter.drawPolygon(screen_outline);
    
    // Draw formation label if enabled
    if (show_formation_labels_ && !formation.formation_label.isEmpty()) {
      QPointF label_pos = FieldToScreen(formation.center_point);
      painter.setPen(formation.formation_color);
      painter.drawText(label_pos, formation.formation_label);
    }
  }
}

void FormationOverlay::RenderPlayerPosition(const PlayerPosition& player)
{
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  
  QPointF screen_pos = FieldToScreen(player.field_position);
  
  // Draw player marker
  QColor marker_color = player.marker_color;
  if (player.player_id == selected_element_id_) {
    marker_color = marker_color.lighter(150);
  } else if (player.player_id == hovered_element_id_) {
    marker_color = marker_color.lighter(120);
  }
  
  painter.setBrush(marker_color);
  painter.setPen(QPen(marker_color.darker(150), 1));
  painter.drawEllipse(screen_pos, player_marker_size_, player_marker_size_);
  
  // Draw confidence indicator if enabled
  if (show_confidence_indicators_ && player.confidence < 1.0) {
    double ring_radius = player_marker_size_ + 2;
    double arc_length = 360.0 * player.confidence;
    
    painter.setPen(QPen(QColor(255, 255, 0), 2));
    painter.drawArc(QRectF(screen_pos.x() - ring_radius, screen_pos.y() - ring_radius,
                          ring_radius * 2, ring_radius * 2), 0, arc_length * 16);
  }
  
  // Draw player label if enabled
  if (show_player_labels_ && !player.position_label.isEmpty()) {
    painter.setPen(Qt::white);
    painter.drawText(screen_pos + QPointF(player_marker_size_ + 2, 0), player.position_label);
  }
}

void FormationOverlay::RenderTriangleVisualization(const TriangleVisualization& triangle)
{
  if (triangle.triangle_points.size() < 3) {
    return;
  }
  
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  
  // Convert triangle points to screen coordinates
  QPolygonF screen_triangle;
  for (const QPointF& point : triangle.triangle_points) {
    screen_triangle.append(FieldToScreen(point));
  }
  
  // Draw triangle outline
  QColor triangle_color = triangle.triangle_color;
  triangle_color.setAlpha(static_cast<int>(overlay_opacity_ * 255));
  
  painter.setPen(QPen(triangle_color, triangle_line_width_));
  painter.setBrush(QBrush(triangle_color, Qt::NoBrush));
  painter.drawPolygon(screen_triangle);
  
  // Draw call label
  if (!triangle.call_label.isEmpty()) {
    QPointF label_pos = FieldToScreen(triangle.call_position);
    painter.setPen(triangle_color);
    painter.drawText(label_pos, triangle.call_label);
  }
  
  // Draw arrows if enabled
  if (triangle.show_arrows) {
    // Generate and draw arrows based on call type
    QList<QPolygonF> arrows = FormationOverlayUtils::GenerateTriangleArrows(
      triangle.call_type, triangle.triangle_points);
    
    painter.setBrush(triangle.arrow_color);
    for (const QPolygonF& arrow : arrows) {
      QPolygonF screen_arrow;
      for (const QPointF& point : arrow) {
        screen_arrow.append(FieldToScreen(point));
      }
      painter.drawPolygon(screen_arrow);
    }
  }
}

void FormationOverlay::RenderMELVisualization(const MELVisualization& mel_viz)
{
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  
  QPointF screen_pos = FieldToScreen(mel_viz.display_position);
  QSizeF screen_size = mel_viz.display_size;
  
  // Draw background
  painter.setBrush(mel_viz.background_color);
  painter.setPen(Qt::NoPen);
  painter.drawRoundedRect(QRectF(screen_pos, screen_size), 5, 5);
  
  // Draw M.E.L. scores
  painter.setPen(mel_viz.text_color);
  painter.setFont(overlay_font_);
  
  QRectF text_rect(screen_pos, screen_size);
  QString mel_text = QString("M.E.L. Score: %1\nMaking: %2\nEfficiency: %3\nLogical: %4")
                    .arg(mel_viz.mel_results.combined_score, 0, 'f', 1)
                    .arg(mel_viz.mel_results.making_score, 0, 'f', 1)
                    .arg(mel_viz.mel_results.efficiency_score, 0, 'f', 1)
                    .arg(mel_viz.mel_results.logical_score, 0, 'f', 1);
  
  painter.drawText(text_rect, Qt::AlignCenter, mel_text);
  
  // Draw progress bars if enabled
  if (mel_viz.show_progress_bars) {
    double bar_height = 4;
    double bar_spacing = 15;
    double bar_y = screen_pos.y() + screen_size.height() - 30;
    
    // Making score bar
    QRectF making_bar(screen_pos.x() + 5, bar_y, 
                     (screen_size.width() - 10) * (mel_viz.mel_results.making_score / 100.0), 
                     bar_height);
    painter.fillRect(making_bar, QColor(100, 150, 255));
    
    // Efficiency score bar
    bar_y += bar_spacing;
    QRectF efficiency_bar(screen_pos.x() + 5, bar_y,
                         (screen_size.width() - 10) * (mel_viz.mel_results.efficiency_score / 100.0),
                         bar_height);
    painter.fillRect(efficiency_bar, QColor(150, 255, 100));
    
    // Logical score bar
    bar_y += bar_spacing;
    QRectF logical_bar(screen_pos.x() + 5, bar_y,
                      (screen_size.width() - 10) * (mel_viz.mel_results.logical_score / 100.0),
                      bar_height);
    painter.fillRect(logical_bar, QColor(255, 150, 100));
  }
}

QPointF FormationOverlay::FieldToScreen(const QPointF& field_coords) const
{
  // Convert field coordinates to screen coordinates
  double screen_x = ((field_coords.x() - field_bounds_.left()) / field_bounds_.width()) * width();
  double screen_y = ((field_coords.y() - field_bounds_.top()) / field_bounds_.height()) * height();
  
  return QPointF(screen_x, screen_y);
}

QPointF FormationOverlay::ScreenToField(const QPointF& screen_coords) const
{
  // Convert screen coordinates to field coordinates
  double field_x = field_bounds_.left() + (screen_coords.x() / width()) * field_bounds_.width();
  double field_y = field_bounds_.top() + (screen_coords.y() / height()) * field_bounds_.height();
  
  return QPointF(field_x, field_y);
}

QColor FormationOverlay::GetFormationColor(FormationType type, double confidence) const
{
  QColor base_color = formation_colors_.value(type, QColor(150, 150, 150));
  
  if (confidence < 1.0) {
    // Adjust alpha based on confidence
    int alpha = static_cast<int>(confidence * 255);
    base_color.setAlpha(alpha);
  }
  
  return base_color;
}

QColor FormationOverlay::GetTriangleColor(TriangleCall call, double urgency) const
{
  QColor base_color = triangle_colors_.value(call, QColor(128, 128, 128));
  
  if (urgency > 0.0) {
    // Intensify color based on urgency
    base_color = base_color.lighter(static_cast<int>(100 + urgency * 50));
  }
  
  return base_color;
}

void FormationOverlay::UpdateFormationShapes()
{
  QMutexLocker locker(&overlay_mutex_);
  
  for (FormationShape& formation : formations_) {
    // Update screen positions for all players
    for (PlayerPosition& player : formation.players) {
      player.screen_position = FieldToScreen(player.field_position);
    }
    
    // Recalculate formation outline and center
    CalculateFormationOutline(formation);
    formation.center_point = FormationOverlayUtils::CalculateFormationCentroid(formation.players);
  }
}

void FormationOverlay::UpdatePlayerPositions()
{
  QMutexLocker locker(&overlay_mutex_);
  
  for (PlayerPosition& player : players_) {
    player.screen_position = FieldToScreen(player.field_position);
  }
}

void FormationOverlay::UpdateTriangleVisualizations()
{
  QMutexLocker locker(&overlay_mutex_);
  
  for (TriangleVisualization& triangle : triangles_) {
    // Update triangle visualization based on current timestamp
    // Could include animation updates, position adjustments, etc.
  }
}

void FormationOverlay::UpdateMELVisualizations()
{
  QMutexLocker locker(&overlay_mutex_);
  
  for (MELVisualization& mel_viz : mel_visualizations_) {
    // Update M.E.L. visualization positions if needed
    mel_viz.display_position = FieldToScreen(mel_viz.display_position);
  }
}

void FormationOverlay::CalculateFormationOutline(FormationShape& formation)
{
  if (formation.players.isEmpty()) {
    return;
  }
  
  formation.formation_outline = FormationOverlayUtils::GenerateFormationOutline(formation.players);
}

PlayerPosition* FormationOverlay::FindPlayerAt(const QPointF& screen_position)
{
  QMutexLocker locker(&overlay_mutex_);
  
  for (PlayerPosition& player : players_) {
    QPointF player_screen_pos = FieldToScreen(player.field_position);
    double distance = QVector2D(screen_position - player_screen_pos).length();
    
    if (distance <= player_marker_size_) {
      return &player;
    }
  }
  
  return nullptr;
}

FormationShape* FormationOverlay::FindFormationAt(const QPointF& screen_position)
{
  QMutexLocker locker(&overlay_mutex_);
  
  QPointF field_position = ScreenToField(screen_position);
  
  for (FormationShape& formation : formations_) {
    if (formation.formation_outline.containsPoint(field_position, Qt::OddEvenFill)) {
      return &formation;
    }
  }
  
  return nullptr;
}

void FormationOverlay::StartAnimation(const QString& element_id, const QString& animation_type,
                                    double target_value, int duration_ms)
{
  if (!animations_enabled_) {
    return;
  }
  
  AnimationState animation;
  animation.animation_id = QString("%1_%2").arg(element_id, animation_type);
  animation.target_element = element_id;
  animation.animation_type = animation_type;
  animation.current_value = 0.0;
  animation.target_value = target_value;
  animation.duration_ms = duration_ms;
  animation.easing_curve = QEasingCurve::InOutQuad;
  animation.loop_animation = (animation_type == "pulse");
  animation.start_time = QDateTime::currentMSecsSinceEpoch();
  
  active_animations_[animation.animation_id] = animation;
  
  qDebug() << "Started animation:" << animation.animation_id
           << "for element:" << element_id
           << "type:" << animation_type;
}

void FormationOverlay::StopAnimation(const QString& element_id)
{
  QMutableMapIterator<QString, AnimationState> it(active_animations_);
  while (it.hasNext()) {
    it.next();
    if (it.value().target_element == element_id) {
      it.remove();
    }
  }
}

void FormationOverlay::OnRenderTimer()
{
  if (real_time_mode_) {
    update(); // Trigger repaint
  }
}

void FormationOverlay::OnAnimationTimer()
{
  if (animations_enabled_ && !active_animations_.isEmpty()) {
    UpdateAnimations();
    update(); // Trigger repaint
  }
}

void FormationOverlay::OnStatisticsTimer()
{
  UpdateOverlayStatistics();
  emit StatisticsUpdated(overlay_statistics_);
}

void FormationOverlay::UpdateAnimations()
{
  qint64 current_time = QDateTime::currentMSecsSinceEpoch();
  
  QMutableMapIterator<QString, AnimationState> it(active_animations_);
  while (it.hasNext()) {
    it.next();
    AnimationState& animation = it.value();
    
    qint64 elapsed_time = current_time - animation.start_time;
    double progress = static_cast<double>(elapsed_time) / animation.duration_ms;
    
    if (progress >= 1.0) {
      if (animation.loop_animation) {
        // Restart animation
        animation.start_time = current_time;
        progress = 0.0;
      } else {
        // Animation complete
        animation.current_value = animation.target_value;
        it.remove();
        continue;
      }
    }
    
    // Apply easing curve
    QEasingCurve curve(animation.easing_curve);
    double eased_progress = curve.valueForProgress(progress);
    
    // Update animation value
    animation.current_value = eased_progress * animation.target_value;
    
    // Apply animation to target element
    UpdateAnimationState(animation);
  }
  
  // Update statistics
  {
    QMutexLocker locker(&stats_mutex_);
    overlay_statistics_.active_animations = active_animations_.size();
  }
}

void FormationOverlay::UpdateAnimationState(AnimationState& state)
{
  // Apply animation state to target element
  if (state.animation_type == "fade") {
    // Apply fade animation (modify alpha)
  } else if (state.animation_type == "scale") {
    // Apply scale animation (modify size)
  } else if (state.animation_type == "pulse") {
    // Apply pulse animation (modify opacity cyclically)
  }
}

void FormationOverlay::CleanupOldElements()
{
  if (!auto_cleanup_enabled_) {
    return;
  }
  
  qint64 current_time = QDateTime::currentMSecsSinceEpoch();
  qint64 cutoff_time = current_time - element_retention_time_ms_;
  
  QMutexLocker locker(&overlay_mutex_);
  
  // Clean up old formations
  QMutableMapIterator<QString, FormationShape> formation_it(formations_);
  while (formation_it.hasNext()) {
    formation_it.next();
    if (formation_it.value().timestamp < cutoff_time) {
      formation_it.remove();
    }
  }
  
  // Clean up old triangles
  QMutableMapIterator<QString, TriangleVisualization> triangle_it(triangles_);
  while (triangle_it.hasNext()) {
    triangle_it.next();
    if (triangle_it.value().timestamp < cutoff_time) {
      triangle_it.remove();
    }
  }
  
  // Clean up old M.E.L. visualizations
  QMutableMapIterator<QString, MELVisualization> mel_it(mel_visualizations_);
  while (mel_it.hasNext()) {
    mel_it.next();
    if (mel_it.value().timestamp < cutoff_time) {
      mel_it.remove();
    }
  }
  
  // Clean up old players
  QMutableMapIterator<QString, PlayerPosition> player_it(players_);
  while (player_it.hasNext()) {
    player_it.next();
    if (player_it.value().timestamp < cutoff_time) {
      player_it.remove();
    }
  }
  
  qDebug() << "Cleaned up old overlay elements";
}

void FormationOverlay::UpdateOverlayStatistics()
{
  QMutexLocker locker(&stats_mutex_);
  
  overlay_statistics_.last_update = QDateTime::currentDateTime();
  
  // Update memory usage estimate
  qint64 formations_memory = formations_.size() * sizeof(FormationShape);
  qint64 triangles_memory = triangles_.size() * sizeof(TriangleVisualization);
  qint64 mel_memory = mel_visualizations_.size() * sizeof(MELVisualization);
  qint64 players_memory = players_.size() * sizeof(PlayerPosition);
  
  overlay_statistics_.memory_usage_bytes = formations_memory + triangles_memory + 
                                          mel_memory + players_memory;
  
  // Update GPU utilization estimate (simplified)
  overlay_statistics_.gpu_utilization = qMin(100.0, 
    (overlay_statistics_.average_render_time_ms / 16.67) * 100.0); // 16.67ms = 60 FPS
}

// FormationOverlayUtils namespace implementation
namespace FormationOverlayUtils {

QPointF CalculateFormationCentroid(const QList<PlayerPosition>& players)
{
  if (players.isEmpty()) {
    return QPointF(0, 0);
  }
  
  double sum_x = 0.0;
  double sum_y = 0.0;
  
  for (const PlayerPosition& player : players) {
    sum_x += player.field_position.x();
    sum_y += player.field_position.y();
  }
  
  return QPointF(sum_x / players.size(), sum_y / players.size());
}

QPolygonF GenerateFormationOutline(const QList<PlayerPosition>& players)
{
  if (players.size() < 3) {
    return QPolygonF();
  }
  
  // Convert player positions to points
  QList<QPointF> points;
  for (const PlayerPosition& player : players) {
    points.append(player.field_position);
  }
  
  // Simple convex hull algorithm (Graham scan)
  // For production, would use a more robust implementation
  QPolygonF hull;
  
  // Find the bottom-most point
  QPointF bottom = points[0];
  int bottom_index = 0;
  for (int i = 1; i < points.size(); ++i) {
    if (points[i].y() < bottom.y() || 
        (points[i].y() == bottom.y() && points[i].x() < bottom.x())) {
      bottom = points[i];
      bottom_index = i;
    }
  }
  
  // Sort points by polar angle with respect to bottom point
  // Simplified implementation
  points.removeAt(bottom_index);
  hull.append(bottom);
  
  // Add remaining points to hull (simplified)
  for (const QPointF& point : points) {
    hull.append(point);
  }
  
  return hull;
}

double CalculateFormationCompactness(const QList<PlayerPosition>& players)
{
  if (players.size() < 2) {
    return 1.0;
  }
  
  // Calculate average distance from centroid
  QPointF centroid = CalculateFormationCentroid(players);
  double total_distance = 0.0;
  
  for (const PlayerPosition& player : players) {
    QVector2D distance_vector(player.field_position - centroid);
    total_distance += distance_vector.length();
  }
  
  double average_distance = total_distance / players.size();
  
  // Normalize to 0-1 range (assuming max reasonable distance is 50 yards)
  return qBound(0.0, 1.0 - (average_distance / 50.0), 1.0);
}

QList<QPolygonF> GenerateTriangleArrows(TriangleCall call, const QList<QPointF>& triangle_points)
{
  QList<QPolygonF> arrows;
  
  if (triangle_points.size() < 3) {
    return arrows;
  }
  
  QPointF center = (triangle_points[0] + triangle_points[1] + triangle_points[2]) / 3.0;
  
  // Generate arrows based on call type
  switch (call) {
    case TriangleCall::StrongSide: {
      // Arrow pointing to strong side
      QPolygonF arrow;
      arrow << center << center + QPointF(10, 0) << center + QPointF(8, 2)
            << center + QPointF(8, -2) << center + QPointF(10, 0);
      arrows.append(arrow);
      break;
    }
    
    case TriangleCall::WeakSide: {
      // Arrow pointing to weak side
      QPolygonF arrow;
      arrow << center << center + QPointF(-10, 0) << center + QPointF(-8, 2)
            << center + QPointF(-8, -2) << center + QPointF(-10, 0);
      arrows.append(arrow);
      break;
    }
    
    case TriangleCall::MiddleHash: {
      // Arrows pointing to middle
      QPolygonF arrow1, arrow2;
      arrow1 << center + QPointF(-5, -5) << center << center + QPointF(-3, -3);
      arrow2 << center + QPointF(5, -5) << center << center + QPointF(3, -3);
      arrows.append(arrow1);
      arrows.append(arrow2);
      break;
    }
    
    default:
      // No specific arrows for other call types
      break;
  }
  
  return arrows;
}

QColor CalculateConfidenceColor(double confidence, const QColor& base_color)
{
  QColor confidence_color = base_color;
  
  if (confidence < 0.5) {
    // Low confidence - red tint
    confidence_color = QColor::fromHsv(0, 255, 255);
  } else if (confidence < 0.8) {
    // Medium confidence - yellow tint
    confidence_color = QColor::fromHsv(60, 255, 255);
  } else {
    // High confidence - green tint
    confidence_color = QColor::fromHsv(120, 255, 255);
  }
  
  // Blend with base color
  int alpha = static_cast<int>(confidence * 255);
  confidence_color.setAlpha(alpha);
  
  return confidence_color;
}

bool ShouldRenderElement(const QRectF& element_bounds, const QRectF& viewport,
                        double opacity_threshold)
{
  // Check if element is visible in viewport
  if (!element_bounds.intersects(viewport)) {
    return false;
  }
  
  // Check if element is large enough to be visible
  double element_area = element_bounds.width() * element_bounds.height();
  double viewport_area = viewport.width() * viewport.height();
  double relative_size = element_area / viewport_area;
  
  return relative_size > opacity_threshold;
}

} // namespace FormationOverlayUtils

} // namespace olive
