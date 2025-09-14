/***
  Apache-Cleats Sports Editor
  Copyright (C) 2024 AnalyzeMyTeam

  Kafka Event Publisher Implementation
  Real-time event streaming for Triangle Defense coordination
***/

#include "kafka_publisher.h"

#include <QDebug>
#include <QCoreApplication>
#include <QJsonArray>
#include <QUuid>
#include <QHostInfo>
#include <QNetworkRequest>
#include <QUrlQuery>
#include <cstdlib>

// Include librdkafka headers
extern "C" {
#include <librdkafka/rdkafka.h>
}

namespace olive {

KafkaPublisher::KafkaPublisher(QObject* parent)
  : QObject(parent)
  , bootstrap_servers_("localhost:9092")
  , client_id_("apache-cleats-publisher")
  , batch_mode_enabled_(true)
  , batch_size_(100)
  , flush_interval_ms_(1000)
  , max_retry_count_(3)
  , reconnect_interval_ms_(5000)
  , is_connected_(false)
  , is_initialized_(false)
  , connection_retry_count_(0)
  , process_timer_(nullptr)
  , flush_timer_(nullptr)
  , reconnect_timer_(nullptr)
  , stats_timer_(nullptr)
  , worker_thread_(nullptr)
  , worker_(nullptr)
  , network_manager_(nullptr)
  , http_fallback_enabled_(false)
  , http_fallback_url_("http://localhost:8080/kafka-proxy")
{
  // Register metatypes for Qt signal/slot system
  qRegisterMetaType<KafkaEvent>("KafkaEvent");
  qRegisterMetaType<EventType>("EventType");
  qRegisterMetaType<EventPriority>("EventPriority");

  SetupTimers();
  
  // Initialize network manager for HTTP fallback
  network_manager_ = new QNetworkAccessManager(this);
}

KafkaPublisher::~KafkaPublisher()
{
  Shutdown();
}

bool KafkaPublisher::Initialize(const QString& bootstrap_servers, const QString& client_id)
{
  if (is_initialized_) {
    qWarning() << "KafkaPublisher already initialized";
    return true;
  }

  bootstrap_servers_ = bootstrap_servers;
  client_id_ = client_id;

  qInfo() << "Initializing Kafka Publisher" 
          << "servers:" << bootstrap_servers_
          << "client_id:" << client_id_;

  StartWorkerThread();

  is_initialized_ = true;
  return true;
}

void KafkaPublisher::Shutdown()
{
  if (!is_initialized_) {
    return;
  }

  qInfo() << "Shutting down Kafka Publisher";

  // Stop timers
  if (process_timer_ && process_timer_->isActive()) {
    process_timer_->stop();
  }
  if (flush_timer_ && flush_timer_->isActive()) {
    flush_timer_->stop();
  }
  if (reconnect_timer_ && reconnect_timer_->isActive()) {
    reconnect_timer_->stop();
  }
  if (stats_timer_ && stats_timer_->isActive()) {
    stats_timer_->stop();
  }

  // Flush remaining events
  FlushEvents();

  // Stop worker thread
  StopWorkerThread();

  is_initialized_ = false;
  is_connected_ = false;

  qInfo() << "Kafka Publisher shutdown complete";
}

bool KafkaPublisher::PublishEvent(const QString& topic, const QJsonObject& data, EventPriority priority)
{
  return PublishStructuredEvent(EventType::SystemMetric, topic, data, QString(), priority);
}

bool KafkaPublisher::PublishStructuredEvent(EventType type, const QString& topic,
                                          const QJsonObject& data, const QString& key,
                                          EventPriority priority)
{
  if (!is_initialized_) {
    qWarning() << "Kafka Publisher not initialized";
    return false;
  }

  // Create event structure
  KafkaEvent event;
  event.type = type;
  event.topic = topic;
  event.key = key.isEmpty() ? GenerateEventKey(type) : key;
  event.priority = priority;
  event.timestamp = QDateTime::currentMSecsSinceEpoch();
  event.source_component = "apache-cleats";

  // Add metadata to data
  QJsonObject enriched_data = data;
  enriched_data["_metadata"] = CreateEventMetadata(type, priority);
  event.data = enriched_data;

  // Validate event
  if (!ValidateEvent(event)) {
    qWarning() << "Invalid event rejected:" << topic;
    return false;
  }

  // Enqueue for processing
  EnqueueEvent(event);

  return true;
}

bool KafkaPublisher::PublishVideoEvent(const QString& event_type, qint64 timestamp,
                                     const QJsonObject& additional_data)
{
  QJsonObject data = additional_data;
  data["event_type"] = event_type;
  data["video_timestamp"] = timestamp;
  data["system_timestamp"] = QDateTime::currentMSecsSinceEpoch();

  return PublishStructuredEvent(EventType::VideoPlayback, "video-events", data,
                               QString("video_%1_%2").arg(event_type).arg(timestamp));
}

bool KafkaPublisher::PublishTriangleDefenseEvent(const QString& call_type, 
                                                const QJsonObject& formation_data,
                                                qint64 video_timestamp, double confidence)
{
  QJsonObject data;
  data["call_type"] = call_type;
  data["formation_data"] = formation_data;
  data["video_timestamp"] = video_timestamp;
  data["confidence"] = confidence;
  data["system_timestamp"] = QDateTime::currentMSecsSinceEpoch();

  return PublishStructuredEvent(EventType::TriangleDefenseCall, "triangle-defense-events", data,
                               QString("triangle_%1_%2").arg(call_type).arg(video_timestamp),
                               EventPriority::High);
}

bool KafkaPublisher::PublishCoachingAlert(const QString& alert_type, const QString& message,
                                        EventPriority priority, const QJsonObject& context)
{
  QJsonObject data = context;
  data["alert_type"] = alert_type;
  data["message"] = message;
  data["timestamp"] = QDateTime::currentMSecsSinceEpoch();

  return PublishStructuredEvent(EventType::CoachingAlert, "coaching-alerts", data,
                               QString("alert_%1").arg(QUuid::createUuid().toString()),
                               priority);
}

bool KafkaPublisher::PublishMELPipelineEvent(const QString& stage, const QString& status,
                                           const QJsonObject& metrics)
{
  QJsonObject data = metrics;
  data["pipeline_stage"] = stage;
  data["status"] = status;
  data["timestamp"] = QDateTime::currentMSecsSinceEpoch();

  return PublishStructuredEvent(EventType::PipelineStatus, "mel-pipeline-status", data,
                               QString("pipeline_%1").arg(stage));
}

QJsonObject KafkaPublisher::GetStatistics() const
{
  QMutexLocker locker(&stats_mutex_);
  
  QJsonObject stats;
  stats["events_published"] = static_cast<qint64>(stats_.events_published);
  stats["events_failed"] = static_cast<qint64>(stats_.events_failed);
  stats["bytes_sent"] = static_cast<qint64>(stats_.bytes_sent);
  stats["queue_size"] = static_cast<qint64>(stats_.queue_size);
  stats["events_per_second"] = stats_.events_per_second;
  stats["is_connected"] = is_connected_;
  stats["uptime_seconds"] = stats_.start_time.secsTo(QDateTime::currentDateTime());
  stats["last_event_timestamp"] = static_cast<qint64>(stats_.last_event_timestamp);

  return stats;
}

void KafkaPublisher::SetBatchMode(bool enabled, int batch_size, int flush_interval_ms)
{
  batch_mode_enabled_ = enabled;
  batch_size_ = batch_size;
  flush_interval_ms_ = flush_interval_ms;

  if (flush_timer_) {
    flush_timer_->setInterval(flush_interval_ms_);
    if (enabled && is_connected_) {
      flush_timer_->start();
    } else {
      flush_timer_->stop();
    }
  }

  qInfo() << "Batch mode" << (enabled ? "enabled" : "disabled")
          << "batch_size:" << batch_size << "flush_interval:" << flush_interval_ms;
}

void KafkaPublisher::SetEventFilter(const QStringList& allowed_topics)
{
  allowed_topics_ = allowed_topics;
  qInfo() << "Event filter updated, allowed topics:" << allowed_topics_;
}

void KafkaPublisher::FlushEvents()
{
  if (worker_) {
    QMetaObject::invokeMethod(worker_, "FlushProducer", Qt::QueuedConnection);
  }
}

void KafkaPublisher::OnConnectionStatusChanged(bool connected)
{
  if (is_connected_ != connected) {
    is_connected_ = connected;
    
    if (connected) {
      qInfo() << "Kafka connection established";
      connection_retry_count_ = 0;
      if (reconnect_timer_ && reconnect_timer_->isActive()) {
        reconnect_timer_->stop();
      }
      if (process_timer_ && !process_timer_->isActive()) {
        process_timer_->start();
      }
      if (batch_mode_enabled_ && flush_timer_ && !flush_timer_->isActive()) {
        flush_timer_->start();
      }
      emit ConnectionEstablished();
    } else {
      qWarning() << "Kafka connection lost";
      if (reconnect_timer_ && !reconnect_timer_->isActive()) {
        reconnect_timer_->start();
      }
      emit ConnectionLost();
    }
  }
}

void KafkaPublisher::OnEventDelivered(const QString& topic, const QString& key, bool success)
{
  QMutexLocker locker(&stats_mutex_);
  
  if (success) {
    stats_.events_published++;
    emit EventPublished(topic, key);
  } else {
    stats_.events_failed++;
    emit EventFailed(topic, "Delivery failed");
  }
  
  stats_.last_event_timestamp = QDateTime::currentMSecsSinceEpoch();
}

void KafkaPublisher::OnError(const QString& error_message, int error_code)
{
  qWarning() << "Kafka error:" << error_message << "code:" << error_code;
  
  // Handle specific error codes
  if (error_code == RD_KAFKA_RESP_ERR__TRANSPORT) {
    HandleConnectionFailure();
  }
}

void KafkaPublisher::ProcessEventQueue()
{
  if (!is_connected_ || !worker_) {
    return;
  }

  QMutexLocker locker(&queue_mutex_);
  
  int processed = 0;
  while (!event_queue_.isEmpty() && processed < batch_size_) {
    KafkaEvent event = event_queue_.dequeue();
    locker.unlock();
    
    // Send to worker thread
    QMetaObject::invokeMethod(worker_, "PublishEvent", Qt::QueuedConnection,
                             Q_ARG(KafkaEvent, event));
    
    processed++;
    locker.relock();
  }
  
  // Update queue size stat
  {
    QMutexLocker stats_locker(&stats_mutex_);
    stats_.queue_size = event_queue_.size();
  }
}

void KafkaPublisher::OnFlushTimer()
{
  if (is_connected_ && worker_) {
    QMetaObject::invokeMethod(worker_, "FlushProducer", Qt::QueuedConnection);
  }
}

void KafkaPublisher::OnReconnectTimer()
{
  if (!is_connected_) {
    AttemptReconnection();
  }
}

void KafkaPublisher::OnStatsTimer()
{
  // Calculate events per second
  {
    QMutexLocker locker(&stats_mutex_);
    qint64 current_time = QDateTime::currentMSecsSinceEpoch();
    qint64 elapsed_seconds = stats_.start_time.secsTo(QDateTime::currentDateTime());
    
    if (elapsed_seconds > 0) {
      stats_.events_per_second = static_cast<double>(stats_.events_published) / elapsed_seconds;
    }
  }
  
  emit StatisticsUpdated(GetStatistics());
}

void KafkaPublisher::SetupTimers()
{
  // Process timer for event queue
  process_timer_ = new QTimer(this);
  process_timer_->setInterval(50); // 50ms = 20 Hz processing
  connect(process_timer_, &QTimer::timeout, this, &KafkaPublisher::ProcessEventQueue);

  // Flush timer for batch mode
  flush_timer_ = new QTimer(this);
  flush_timer_->setInterval(flush_interval_ms_);
  connect(flush_timer_, &QTimer::timeout, this, &KafkaPublisher::OnFlushTimer);

  // Reconnect timer
  reconnect_timer_ = new QTimer(this);
  reconnect_timer_->setInterval(reconnect_interval_ms_);
  connect(reconnect_timer_, &QTimer::timeout, this, &KafkaPublisher::OnReconnectTimer);

  // Statistics timer
  stats_timer_ = new QTimer(this);
  stats_timer_->setInterval(10000); // 10 seconds
  connect(stats_timer_, &QTimer::timeout, this, &KafkaPublisher::OnStatsTimer);
  stats_timer_->start();
}

void KafkaPublisher::StartWorkerThread()
{
  if (worker_thread_) {
    return;
  }

  worker_thread_ = new QThread(this);
  worker_ = new KafkaProducerWorker(bootstrap_servers_, client_id_);
  worker_->moveToThread(worker_thread_);

  // Connect signals
  connect(worker_thread_, &QThread::started, worker_, &KafkaProducerWorker::Initialize);
  connect(worker_thread_, &QThread::finished, worker_, &QObject::deleteLater);
  connect(worker_, &KafkaProducerWorker::Initialized, this, &KafkaPublisher::OnConnectionStatusChanged);
  connect(worker_, &KafkaProducerWorker::EventDelivered, this, &KafkaPublisher::OnEventDelivered);
  connect(worker_, &KafkaProducerWorker::ErrorOccurred, this, &KafkaPublisher::OnError);
  connect(worker_, &KafkaProducerWorker::ConnectionStatusChanged, this, &KafkaPublisher::OnConnectionStatusChanged);

  worker_thread_->start();
}

void KafkaPublisher::StopWorkerThread()
{
  if (!worker_thread_) {
    return;
  }

  if (worker_) {
    QMetaObject::invokeMethod(worker_, "Shutdown", Qt::QueuedConnection);
  }

  worker_thread_->quit();
  worker_thread_->wait(5000);

  worker_thread_ = nullptr;
  worker_ = nullptr;
}

bool KafkaPublisher::ValidateEvent(const KafkaEvent& event)
{
  // Check topic name
  if (event.topic.isEmpty() || !KafkaUtils::IsValidTopicName(event.topic)) {
    qWarning() << "Invalid topic name:" << event.topic;
    return false;
  }

  // Check topic filter
  if (!allowed_topics_.isEmpty() && !allowed_topics_.contains(event.topic)) {
    qDebug() << "Topic filtered out:" << event.topic;
    return false;
  }

  // Check data size
  qint64 size = KafkaUtils::CalculateEventSize(event);
  if (size > 1024 * 1024) { // 1MB limit
    qWarning() << "Event too large:" << size << "bytes";
    return false;
  }

  return true;
}

void KafkaPublisher::EnqueueEvent(const KafkaEvent& event)
{
  QMutexLocker locker(&queue_mutex_);
  
  // Drop low priority events if queue is full
  if (event_queue_.size() > 10000 && event.priority == EventPriority::Low) {
    qWarning() << "Dropping low priority event, queue full";
    return;
  }

  event_queue_.enqueue(event);
  
  // Start processing if not running
  if (is_connected_ && process_timer_ && !process_timer_->isActive()) {
    process_timer_->start();
  }
}

void KafkaPublisher::UpdateStatistics(const KafkaEvent& event, bool success)
{
  QMutexLocker locker(&stats_mutex_);
  
  if (success) {
    stats_.events_published++;
    stats_.bytes_sent += KafkaUtils::CalculateEventSize(event);
  } else {
    stats_.events_failed++;
  }
  
  stats_.last_event_timestamp = event.timestamp;
}

QJsonObject KafkaPublisher::CreateEventMetadata(EventType type, EventPriority priority)
{
  QJsonObject metadata;
  metadata["event_type"] = KafkaUtils::EventTypeToString(type);
  metadata["priority"] = KafkaUtils::EventPriorityToString(priority);
  metadata["source"] = "apache-cleats";
  metadata["hostname"] = QHostInfo::localHostName();
  metadata["version"] = QCoreApplication::applicationVersion();
  metadata["timestamp"] = QDateTime::currentMSecsSinceEpoch();

  return metadata;
}

QString KafkaPublisher::GenerateEventKey(EventType type, const QString& custom_key)
{
  if (!custom_key.isEmpty()) {
    return custom_key;
  }

  QString base_key = KafkaUtils::EventTypeToString(type);
  QString uuid = QUuid::createUuid().toString().remove('{').remove('}');
  
  return QString("%1_%2").arg(base_key, uuid.left(8));
}

void KafkaPublisher::HandleConnectionFailure()
{
  is_connected_ = false;
  connection_retry_count_++;
  
  qWarning() << "Connection failure, retry count:" << connection_retry_count_;
  
  if (!reconnect_timer_->isActive()) {
    reconnect_timer_->start();
  }
}

void KafkaPublisher::AttemptReconnection()
{
  if (is_connected_) {
    return;
  }

  qInfo() << "Attempting Kafka reconnection, attempt:" << connection_retry_count_;
  last_connection_attempt_ = QDateTime::currentDateTime();

  if (worker_) {
    QMetaObject::invokeMethod(worker_, "Initialize", Qt::QueuedConnection);
  }
}

// KafkaProducerWorker Implementation
KafkaProducerWorker::KafkaProducerWorker(const QString& bootstrap_servers, const QString& client_id)
  : QObject(nullptr)
  , bootstrap_servers_(bootstrap_servers)
  , client_id_(client_id)
  , producer_(nullptr)
  , config_(nullptr)
  , is_initialized_(false)
{
}

KafkaProducerWorker::~KafkaProducerWorker()
{
  Shutdown();
}

void KafkaProducerWorker::Initialize()
{
  if (is_initialized_) {
    return;
  }

  qInfo() << "Initializing Kafka producer worker";

  SetupProducer();
  
  is_initialized_ = true;
  emit Initialized(producer_ != nullptr);
  emit ConnectionStatusChanged(producer_ != nullptr);
}

void KafkaProducerWorker::Shutdown()
{
  if (!is_initialized_) {
    return;
  }

  qInfo() << "Shutting down Kafka producer worker";

  CleanupProducer();
  
  is_initialized_ = false;
  emit ConnectionStatusChanged(false);
}

void KafkaProducerWorker::PublishEvent(const KafkaEvent& event)
{
  if (!producer_) {
    emit EventDelivered(event.topic, event.key, false);
    return;
  }

  QMutexLocker locker(&producer_mutex_);

  // Convert event data to JSON
  QJsonDocument doc(event.data);
  QByteArray payload = doc.toJson(QJsonDocument::Compact);

  // Publish to Kafka
  rd_kafka_resp_err_t err = rd_kafka_producev(
    static_cast<rd_kafka_t*>(producer_),
    RD_KAFKA_V_TOPIC(event.topic.toUtf8().constData()),
    RD_KAFKA_V_KEY(event.key.toUtf8().constData(), event.key.toUtf8().length()),
    RD_KAFKA_V_VALUE(payload.constData(), payload.length()),
    RD_KAFKA_V_OPAQUE(this),
    RD_KAFKA_V_END
  );

  if (err) {
    qWarning() << "Failed to publish event:" << rd_kafka_err2str(err);
    emit EventDelivered(event.topic, event.key, false);
    emit ErrorOccurred(QString::fromUtf8(rd_kafka_err2str(err)), err);
  }
}

void KafkaProducerWorker::FlushProducer()
{
  if (producer_) {
    QMutexLocker locker(&producer_mutex_);
    rd_kafka_flush(static_cast<rd_kafka_t*>(producer_), 1000); // 1 second timeout
  }
}

void KafkaProducerWorker::SetupProducer()
{
  char errstr[512];

  // Create configuration
  config_ = rd_kafka_conf_new();

  // Set bootstrap servers
  if (rd_kafka_conf_set(static_cast<rd_kafka_conf_t*>(config_), "bootstrap.servers",
                       bootstrap_servers_.toUtf8().constData(), errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
    qCritical() << "Failed to set bootstrap.servers:" << errstr;
    return;
  }

  // Set client ID
  if (rd_kafka_conf_set(static_cast<rd_kafka_conf_t*>(config_), "client.id",
                       client_id_.toUtf8().constData(), errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
    qWarning() << "Failed to set client.id:" << errstr;
  }

  // Set callbacks
  rd_kafka_conf_set_dr_msg_cb(static_cast<rd_kafka_conf_t*>(config_), DeliveryReportCallback);
  rd_kafka_conf_set_error_cb(static_cast<rd_kafka_conf_t*>(config_), ErrorCallback);

  // Additional producer settings
  rd_kafka_conf_set(static_cast<rd_kafka_conf_t*>(config_), "acks", "1", nullptr, 0);
  rd_kafka_conf_set(static_cast<rd_kafka_conf_t*>(config_), "retries", "3", nullptr, 0);
  rd_kafka_conf_set(static_cast<rd_kafka_conf_t*>(config_), "batch.size", "16384", nullptr, 0);
  rd_kafka_conf_set(static_cast<rd_kafka_conf_t*>(config_), "linger.ms", "50", nullptr, 0);

  // Create producer
  producer_ = rd_kafka_new(RD_KAFKA_PRODUCER, static_cast<rd_kafka_conf_t*>(config_), errstr, sizeof(errstr));
  if (!producer_) {
    qCritical() << "Failed to create Kafka producer:" << errstr;
    return;
  }

  qInfo() << "Kafka producer created successfully";
}

void KafkaProducerWorker::CleanupProducer()
{
  if (producer_) {
    QMutexLocker locker(&producer_mutex_);
    
    // Flush remaining messages
    rd_kafka_flush(static_cast<rd_kafka_t*>(producer_), 5000);
    
    // Destroy producer
    rd_kafka_destroy(static_cast<rd_kafka_t*>(producer_));
    producer_ = nullptr;
  }

  config_ = nullptr; // Config is freed by rd_kafka_destroy
}

void KafkaProducerWorker::DeliveryReportCallback(void* producer, const void* message, void* opaque)
{
  Q_UNUSED(producer)
  
  const rd_kafka_message_t* msg = static_cast<const rd_kafka_message_t*>(message);
  KafkaProducerWorker* worker = static_cast<KafkaProducerWorker*>(opaque);

  if (worker) {
    QString topic = QString::fromUtf8(rd_kafka_topic_name(msg->rkt));
    QString key = QString::fromUtf8(static_cast<const char*>(msg->key), msg->key_len);
    bool success = (msg->err == RD_KAFKA_RESP_ERR_NO_ERROR);
    
    QMetaObject::invokeMethod(worker, "EventDelivered", Qt::QueuedConnection,
                             Q_ARG(QString, topic), Q_ARG(QString, key), Q_ARG(bool, success));
  }
}

void KafkaProducerWorker::ErrorCallback(void* producer, int error_code, const char* reason, void* opaque)
{
  Q_UNUSED(producer)
  
  KafkaProducerWorker* worker = static_cast<KafkaProducerWorker*>(opaque);
  
  if (worker) {
    QString error_msg = QString::fromUtf8(reason);
    QMetaObject::invokeMethod(worker, "ErrorOccurred", Qt::QueuedConnection,
                             Q_ARG(QString, error_msg), Q_ARG(int, error_code));
  }
}

// KafkaUtils Implementation
namespace KafkaUtils {

QString EventTypeToString(EventType type)
{
  switch (type) {
    case EventType::VideoPlayback: return "video_playback";
    case EventType::VideoSeek: return "video_seek";
    case EventType::DashboardInteraction: return "dashboard_interaction";
    case EventType::FormationUpdate: return "formation_update";
    case EventType::TriangleDefenseCall: return "triangle_defense_call";
    case EventType::CoachingAlert: return "coaching_alert";
    case EventType::PipelineStatus: return "pipeline_status";
    case EventType::SystemMetric: return "system_metric";
    default: return "unknown";
  }
}

QString EventPriorityToString(EventPriority priority)
{
  switch (priority) {
    case EventPriority::Low: return "low";
    case EventPriority::Normal: return "normal";
    case EventPriority::High: return "high";
    case EventPriority::Critical: return "critical";
    default: return "normal";
  }
}

QString GenerateTopicName(EventType type)
{
  QString base = EventTypeToString(type);
  return base.replace('_', '-') + "-events";
}

QJsonObject CreateStandardEvent(EventType type, const QString& source, const QJsonObject& payload)
{
  QJsonObject event;
  event["event_type"] = EventTypeToString(type);
  event["source"] = source;
  event["timestamp"] = QDateTime::currentMSecsSinceEpoch();
  event["payload"] = payload;
  
  return event;
}

bool IsValidTopicName(const QString& topic)
{
  if (topic.isEmpty() || topic.length() > 249) {
    return false;
  }
  
  // Kafka topic name validation
  QRegExp regex("^[a-zA-Z0-9._-]+$");
  return regex.exactMatch(topic);
}

qint64 CalculateEventSize(const KafkaEvent& event)
{
  QJsonDocument doc(event.data);
  QByteArray data = doc.toJson(QJsonDocument::Compact);
  
  return event.topic.toUtf8().length() + 
         event.key.toUtf8().length() + 
         data.length();
}

} // namespace KafkaUtils

} // namespace olive
