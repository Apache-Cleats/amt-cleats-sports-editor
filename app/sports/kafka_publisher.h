/***
  Apache-Cleats Sports Editor
  Copyright (C) 2024 AnalyzeMyTeam

  Kafka Event Publisher for Video-Dashboard Integration
  Real-time event streaming for Triangle Defense coordination
***/

#ifndef KAFKAPUBLISHER_H
#define KAFKAPUBLISHER_H

#include <QObject>
#include <QTimer>
#include <QQueue>
#include <QMutex>
#include <QThread>
#include <QJsonObject>
#include <QJsonDocument>
#include <QString>
#include <QStringList>
#include <QDateTime>
#include <QNetworkAccessManager>
#include <QNetworkReply>

namespace olive {

class KafkaProducerWorker;

/**
 * @brief Event types for Kafka publishing
 */
enum class EventType {
  VideoPlayback,
  VideoSeek,
  DashboardInteraction,
  FormationUpdate,
  TriangleDefenseCall,
  CoachingAlert,
  PipelineStatus,
  SystemMetric
};

/**
 * @brief Event priority levels
 */
enum class EventPriority {
  Low = 0,
  Normal = 1,
  High = 2,
  Critical = 3
};

/**
 * @brief Event data structure for Kafka publishing
 */
struct KafkaEvent {
  QString topic;
  QString key;
  QJsonObject data;
  EventType type;
  EventPriority priority;
  qint64 timestamp;
  int retry_count;
  QString source_component;
  
  KafkaEvent() 
    : type(EventType::SystemMetric)
    , priority(EventPriority::Normal)
    , timestamp(QDateTime::currentMSecsSinceEpoch())
    , retry_count(0)
    , source_component("apache-cleats") {}
};

/**
 * @brief High-performance Kafka publisher for real-time video events
 */
class KafkaPublisher : public QObject
{
  Q_OBJECT

public:
  explicit KafkaPublisher(QObject* parent = nullptr);
  virtual ~KafkaPublisher();

  /**
   * @brief Initialize Kafka connection
   */
  bool Initialize(const QString& bootstrap_servers = "localhost:9092",
                  const QString& client_id = "apache-cleats-publisher");

  /**
   * @brief Shutdown Kafka connection gracefully
   */
  void Shutdown();

  /**
   * @brief Publish event to specific topic
   */
  bool PublishEvent(const QString& topic, const QJsonObject& data,
                   EventPriority priority = EventPriority::Normal);

  /**
   * @brief Publish structured event with metadata
   */
  bool PublishStructuredEvent(EventType type, const QString& topic,
                             const QJsonObject& data, const QString& key = QString(),
                             EventPriority priority = EventPriority::Normal);

  /**
   * @brief Publish video timeline event
   */
  bool PublishVideoEvent(const QString& event_type, qint64 timestamp,
                        const QJsonObject& additional_data = QJsonObject());

  /**
   * @brief Publish Triangle Defense event
   */
  bool PublishTriangleDefenseEvent(const QString& call_type, const QJsonObject& formation_data,
                                  qint64 video_timestamp, double confidence = 0.0);

  /**
   * @brief Publish coaching alert
   */
  bool PublishCoachingAlert(const QString& alert_type, const QString& message,
                           EventPriority priority, const QJsonObject& context = QJsonObject());

  /**
   * @brief Publish M.E.L. pipeline status
   */
  bool PublishMELPipelineEvent(const QString& stage, const QString& status,
                              const QJsonObject& metrics = QJsonObject());

  /**
   * @brief Get connection status
   */
  bool IsConnected() const { return is_connected_; }

  /**
   * @brief Get producer statistics
   */
  QJsonObject GetStatistics() const;

  /**
   * @brief Enable/disable batch publishing
   */
  void SetBatchMode(bool enabled, int batch_size = 100, int flush_interval_ms = 1000);

  /**
   * @brief Set event filtering
   */
  void SetEventFilter(const QStringList& allowed_topics);

  /**
   * @brief Force flush all pending events
   */
  void FlushEvents();

public slots:
  /**
   * @brief Handle connection status changes
   */
  void OnConnectionStatusChanged(bool connected);

  /**
   * @brief Handle delivery reports
   */
  void OnEventDelivered(const QString& topic, const QString& key, bool success);

  /**
   * @brief Handle errors
   */
  void OnError(const QString& error_message, int error_code);

signals:
  /**
   * @brief Event publishing signals
   */
  void EventPublished(const QString& topic, const QString& key);
  void EventFailed(const QString& topic, const QString& error);
  void ConnectionEstablished();
  void ConnectionLost();
  void StatisticsUpdated(const QJsonObject& stats);

private slots:
  void ProcessEventQueue();
  void OnFlushTimer();
  void OnReconnectTimer();
  void OnStatsTimer();

private:
  void SetupTimers();
  void StartWorkerThread();
  void StopWorkerThread();
  bool ValidateEvent(const KafkaEvent& event);
  void EnqueueEvent(const KafkaEvent& event);
  void UpdateStatistics(const KafkaEvent& event, bool success);
  QJsonObject CreateEventMetadata(EventType type, EventPriority priority);
  QString GenerateEventKey(EventType type, const QString& custom_key = QString());
  void HandleConnectionFailure();
  void AttemptReconnection();

  // Configuration
  QString bootstrap_servers_;
  QString client_id_;
  QStringList allowed_topics_;
  bool batch_mode_enabled_;
  int batch_size_;
  int flush_interval_ms_;
  int max_retry_count_;
  int reconnect_interval_ms_;
  
  // Connection state
  bool is_connected_;
  bool is_initialized_;
  QDateTime last_connection_attempt_;
  int connection_retry_count_;
  
  // Event queue and processing
  QQueue<KafkaEvent> event_queue_;
  QMutex queue_mutex_;
  QTimer* process_timer_;
  QTimer* flush_timer_;
  QTimer* reconnect_timer_;
  QTimer* stats_timer_;
  
  // Worker thread for Kafka operations
  QThread* worker_thread_;
  KafkaProducerWorker* worker_;
  
  // Statistics
  mutable QMutex stats_mutex_;
  struct Statistics {
    qint64 events_published;
    qint64 events_failed;
    qint64 bytes_sent;
    qint64 queue_size;
    qint64 last_event_timestamp;
    double events_per_second;
    QDateTime start_time;
    
    Statistics() : events_published(0), events_failed(0), bytes_sent(0),
                   queue_size(0), last_event_timestamp(0), events_per_second(0.0),
                   start_time(QDateTime::currentDateTime()) {}
  } stats_;
  
  // Network manager for HTTP fallback
  QNetworkAccessManager* network_manager_;
  bool http_fallback_enabled_;
  QString http_fallback_url_;
};

/**
 * @brief Worker thread for Kafka producer operations
 */
class KafkaProducerWorker : public QObject
{
  Q_OBJECT

public:
  explicit KafkaProducerWorker(const QString& bootstrap_servers, const QString& client_id);
  virtual ~KafkaProducerWorker();

public slots:
  void Initialize();
  void Shutdown();
  void PublishEvent(const KafkaEvent& event);
  void FlushProducer();

signals:
  void Initialized(bool success);
  void EventDelivered(const QString& topic, const QString& key, bool success);
  void ErrorOccurred(const QString& error, int error_code);
  void ConnectionStatusChanged(bool connected);

private:
  void SetupProducer();
  void CleanupProducer();
  static void DeliveryReportCallback(void* producer, const void* message, void* opaque);
  static void ErrorCallback(void* producer, int error_code, const char* reason, void* opaque);

  QString bootstrap_servers_;
  QString client_id_;
  void* producer_; // rd_kafka_t*
  void* config_;   // rd_kafka_conf_t*
  bool is_initialized_;
  QMutex producer_mutex_;
};

/**
 * @brief Utility functions for Kafka event publishing
 */
namespace KafkaUtils {

/**
 * @brief Convert EventType to string
 */
QString EventTypeToString(EventType type);

/**
 * @brief Convert EventPriority to string
 */
QString EventPriorityToString(EventPriority priority);

/**
 * @brief Generate topic name based on event type
 */
QString GenerateTopicName(EventType type);

/**
 * @brief Create standardized event structure
 */
QJsonObject CreateStandardEvent(EventType type, const QString& source,
                               const QJsonObject& payload);

/**
 * @brief Validate topic name format
 */
bool IsValidTopicName(const QString& topic);

/**
 * @brief Calculate event payload size
 */
qint64 CalculateEventSize(const KafkaEvent& event);

} // namespace KafkaUtils

} // namespace olive

Q_DECLARE_METATYPE(olive::KafkaEvent)
Q_DECLARE_METATYPE(olive::EventType)
Q_DECLARE_METATYPE(olive::EventPriority)

#endif // KAFKAPUBLISHER_H
