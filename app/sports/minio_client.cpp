/***
  Apache-Cleats Sports Editor
  Copyright (C) 2024 AnalyzeMyTeam

  MinIO Client Implementation
  High-performance video storage and Dynamic Fabricator result storage
***/

#include "minio_client.h"

#include <QDebug>
#include <QNetworkRequest>
#include <QHttpMultiPart>
#include <QHttpPart>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QUuid>
#include <QStandardPaths>
#include <QProcess>
#include <QRegularExpression>
#include <QMimeDatabase>
#include <QImageReader>
#include <QBuffer>
#include <QtMath>

// Include FFmpeg headers for video analysis
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

namespace olive {

// MinIOUploadOperation Implementation
MinIOUploadOperation::MinIOUploadOperation(const QString& operation_id, const QString& file_path,
                                         const QString& bucket, const QString& object_key,
                                         QObject* parent)
  : QObject(parent)
  , operation_id_(operation_id)
  , file_path_(file_path)
  , bucket_name_(bucket)
  , object_key_(object_key)
  , current_reply_(nullptr)
  , network_manager_(nullptr)
  , retry_count_(0)
{
  progress_.operation_id = operation_id;
  progress_.file_path = file_path;
  progress_.status = "pending";
  progress_.start_time = QDateTime::currentDateTime();
  
  network_manager_ = new QNetworkAccessManager(this);
}

void MinIOUploadOperation::Start()
{
  QFileInfo file_info(file_path_);
  if (!file_info.exists() || !file_info.isReadable()) {
    progress_.status = "failed";
    progress_.error_message = "File not found or not readable";
    emit Failed(progress_.error_message);
    return;
  }
  
  progress_.total_bytes = file_info.size();
  progress_.status = "uploading";
  progress_.start_time = QDateTime::currentDateTime();
  
  // Create multipart upload request
  QFile* file = new QFile(file_path_);
  if (!file->open(QIODevice::ReadOnly)) {
    progress_.status = "failed";
    progress_.error_message = "Cannot open file for reading";
    emit Failed(progress_.error_message);
    file->deleteLater();
    return;
  }
  
  // Create request (this would be handled by parent MinIOClient in practice)
  QNetworkRequest request;
  request.setUrl(QUrl(QString("https://storage.endpoint/%1/%2").arg(bucket_name_, object_key_)));
  request.setHeader(QNetworkRequest::ContentTypeHeader, "application/octet-stream");
  request.setHeader(QNetworkRequest::ContentLengthHeader, progress_.total_bytes);
  
  current_reply_ = network_manager_->put(request, file);
  file->setParent(current_reply_); // Auto-delete with reply
  
  connect(current_reply_, &QNetworkReply::uploadProgress,
          this, &MinIOUploadOperation::OnUploadProgress);
  connect(current_reply_, &QNetworkReply::finished,
          this, &MinIOUploadOperation::OnUploadFinished);
  
  qInfo() << "Started upload operation:" << operation_id_ << "file:" << file_path_;
}

void MinIOUploadOperation::Cancel()
{
  if (current_reply_) {
    current_reply_->abort();
    progress_.status = "cancelled";
    qInfo() << "Cancelled upload operation:" << operation_id_;
  }
}

void MinIOUploadOperation::Retry()
{
  if (retry_count_ < MAX_RETRIES) {
    retry_count_++;
    progress_.status = "pending";
    progress_.error_message.clear();
    qInfo() << "Retrying upload operation:" << operation_id_ << "attempt:" << retry_count_;
    Start();
  } else {
    progress_.status = "failed";
    progress_.error_message = "Maximum retry attempts exceeded";
    emit Failed(progress_.error_message);
  }
}

void MinIOUploadOperation::OnUploadProgress(qint64 bytes_sent, qint64 bytes_total)
{
  progress_.bytes_transferred = bytes_sent;
  progress_.total_bytes = bytes_total;
  progress_.percentage = (bytes_total > 0) ? (static_cast<double>(bytes_sent) / bytes_total) * 100.0 : 0.0;
  progress_.last_update = QDateTime::currentDateTime();
  
  emit ProgressUpdated(progress_);
}

void MinIOUploadOperation::OnUploadFinished()
{
  if (!current_reply_) {
    return;
  }
  
  if (current_reply_->error() == QNetworkReply::NoError) {
    progress_.status = "completed";
    progress_.percentage = 100.0;
    progress_.last_update = QDateTime::currentDateTime();
    
    // Create metadata from response
    VideoMetadata metadata;
    metadata.file_id = operation_id_;
    metadata.original_filename = QFileInfo(file_path_).fileName();
    metadata.bucket_name = bucket_name_;
    metadata.object_key = object_key_;
    metadata.file_size = progress_.total_bytes;
    metadata.upload_timestamp = QDateTime::currentDateTime();
    metadata.etag = current_reply_->rawHeader("ETag");
    
    emit Completed(metadata);
    qInfo() << "Upload completed:" << operation_id_;
  } else {
    progress_.status = "failed";
    progress_.error_message = current_reply_->errorString();
    emit Failed(progress_.error_message);
    qWarning() << "Upload failed:" << operation_id_ << progress_.error_message;
  }
  
  current_reply_->deleteLater();
  current_reply_ = nullptr;
}

// MinIOClient Implementation
MinIOClient::MinIOClient(QObject* parent)
  : QObject(parent)
  , endpoint_("localhost:9000")
  , access_key_("")
  , secret_key_("")
  , use_ssl_(false)
  , is_connected_(false)
  , is_initialized_(false)
  , multipart_enabled_(true)
  , multipart_part_size_(5 * 1024 * 1024) // 5MB
  , connect_timeout_ms_(10000)
  , transfer_timeout_ms_(300000)
  , video_analysis_enabled_(false)
  , network_manager_(nullptr)
  , stats_timer_(nullptr)
  , cleanup_timer_(nullptr)
{
  SetupNetworking();
  SetupTimers();
}

MinIOClient::~MinIOClient()
{
  Shutdown();
}

bool MinIOClient::Initialize(const QString& endpoint, const QString& access_key,
                           const QString& secret_key, bool use_ssl)
{
  if (is_initialized_) {
    qWarning() << "MinIOClient already initialized";
    return true;
  }

  endpoint_ = endpoint;
  access_key_ = access_key;
  secret_key_ = secret_key;
  use_ssl_ = use_ssl;

  if (!ValidateConfiguration()) {
    qCritical() << "Invalid MinIO configuration";
    return false;
  }

  qInfo() << "Initializing MinIO Client"
          << "endpoint:" << endpoint_
          << "SSL:" << use_ssl_;

  // Test connection
  if (CreateBucket("videos")) {
    is_connected_ = true;
    is_initialized_ = true;
    
    if (stats_timer_) {
      stats_timer_->start();
    }
    
    emit Connected();
    qInfo() << "MinIO Client initialized successfully";
    return true;
  } else {
    qWarning() << "Failed to connect to MinIO";
    HandleConnectionFailure("Connection test failed");
    return false;
  }
}

void MinIOClient::Shutdown()
{
  if (!is_initialized_) {
    return;
  }

  qInfo() << "Shutting down MinIO Client";

  // Cancel all active operations
  CancelAllTransfers();

  // Stop timers
  if (stats_timer_ && stats_timer_->isActive()) {
    stats_timer_->stop();
  }
  if (cleanup_timer_ && cleanup_timer_->isActive()) {
    cleanup_timer_->stop();
  }

  // Clear caches
  {
    QMutexLocker locker(&cache_mutex_);
    video_cache_.clear();
    fabricator_cache_.clear();
    presigned_url_cache_.clear();
  }

  is_initialized_ = false;
  is_connected_ = false;
  
  emit Disconnected();
  qInfo() << "MinIO Client shutdown complete";
}

QString MinIOClient::UploadVideoFile(const QString& file_path, const QString& bucket,
                                   const QJsonObject& metadata)
{
  if (!is_connected_) {
    qWarning() << "MinIO client not connected";
    return QString();
  }

  QFileInfo file_info(file_path);
  if (!file_info.exists()) {
    qWarning() << "Video file not found:" << file_path;
    return QString();
  }

  // Validate video format
  if (!MinIOUtils::IsValidVideoFormat(file_path)) {
    qWarning() << "Invalid video format:" << file_path;
    return QString();
  }

  QString operation_id = GenerateOperationId();
  QString object_key = MinIOUtils::GenerateVideoObjectKey(file_info.fileName());

  // Create upload operation
  MinIOUploadOperation* upload_op = new MinIOUploadOperation(
    operation_id, file_path, bucket, object_key, this);

  {
    QMutexLocker locker(&operations_mutex_);
    active_uploads_[operation_id] = upload_op;
  }

  // Connect signals
  connect(upload_op, &MinIOUploadOperation::ProgressUpdated,
          this, [this, operation_id](const TransferProgress& progress) {
            OnTransferProgress(operation_id, progress);
          });

  connect(upload_op, &MinIOUploadOperation::Completed,
          this, [this, operation_id, file_path](const VideoMetadata& metadata) {
            // Process video metadata
            VideoMetadata enriched_metadata = metadata;
            ProcessVideoFile(file_path, enriched_metadata);
            UpdateVideoCache(enriched_metadata);
            
            OnTransferCompleted(operation_id);
            emit VideoUploaded(enriched_metadata);
            
            qInfo() << "Video uploaded successfully:" << metadata.original_filename;
          });

  connect(upload_op, &MinIOUploadOperation::Failed,
          this, [this, operation_id](const QString& error) {
            OnTransferFailed(operation_id, error);
          });

  // Start upload
  upload_op->Start();

  qInfo() << "Started video upload:" << operation_id << "file:" << file_info.fileName();
  return operation_id;
}

QString MinIOClient::UploadFabricatorResult(const QString& file_path, const QString& video_file_id,
                                          const QString& result_type, qint64 video_timestamp,
                                          const QJsonObject& processing_metadata)
{
  if (!is_connected_) {
    qWarning() << "MinIO client not connected";
    return QString();
  }

  QFileInfo file_info(file_path);
  if (!file_info.exists()) {
    qWarning() << "Fabricator result file not found:" << file_path;
    return QString();
  }

  QString operation_id = GenerateOperationId();
  QString object_key = MinIOUtils::GenerateFabricatorObjectKey(video_file_id, result_type, video_timestamp);
  QString bucket = "fabricator-results";

  // Create upload operation
  MinIOUploadOperation* upload_op = new MinIOUploadOperation(
    operation_id, file_path, bucket, object_key, this);

  {
    QMutexLocker locker(&operations_mutex_);
    active_uploads_[operation_id] = upload_op;
  }

  // Connect signals
  connect(upload_op, &MinIOUploadOperation::Completed,
          this, [this, operation_id, video_file_id, result_type, video_timestamp, processing_metadata]
          (const VideoMetadata& upload_metadata) {
            // Create Fabricator result metadata
            FabricatorResult result;
            result.result_id = operation_id;
            result.video_file_id = video_file_id;
            result.result_type = result_type;
            result.bucket_name = upload_metadata.bucket_name;
            result.object_key = upload_metadata.object_key;
            result.file_size = upload_metadata.file_size;
            result.generation_timestamp = QDateTime::currentDateTime();
            result.video_timestamp = video_timestamp;
            result.processing_metadata = processing_metadata;
            
            UpdateFabricatorCache(result);
            OnTransferCompleted(operation_id);
            emit FabricatorResultUploaded(result);
            
            qInfo() << "Fabricator result uploaded:" << result_type << "for video:" << video_file_id;
          });

  connect(upload_op, &MinIOUploadOperation::Failed,
          this, [this, operation_id](const QString& error) {
            OnTransferFailed(operation_id, error);
          });

  // Start upload
  upload_op->Start();

  qInfo() << "Started fabricator result upload:" << operation_id << "type:" << result_type;
  return operation_id;
}

QString MinIOClient::DownloadFile(const QString& bucket, const QString& object_key,
                                const QString& local_path)
{
  if (!is_connected_) {
    qWarning() << "MinIO client not connected";
    return QString();
  }

  QString operation_id = GenerateOperationId();
  
  // Create download request
  QNetworkRequest request = CreateRequest("GET", bucket, object_key);
  
  QNetworkReply* reply = network_manager_->get(request);
  
  {
    QMutexLocker locker(&operations_mutex_);
    active_downloads_[operation_id] = reply;
  }

  // Create progress tracking
  TransferProgress progress;
  progress.operation_id = operation_id;
  progress.file_path = local_path;
  progress.status = "downloading";
  progress.start_time = QDateTime::currentDateTime();

  connect(reply, &QNetworkReply::downloadProgress,
          this, [this, operation_id](qint64 bytes_received, qint64 bytes_total) {
            TransferProgress progress;
            progress.operation_id = operation_id;
            progress.bytes_transferred = bytes_received;
            progress.total_bytes = bytes_total;
            progress.percentage = (bytes_total > 0) ? 
              (static_cast<double>(bytes_received) / bytes_total) * 100.0 : 0.0;
            progress.status = "downloading";
            progress.last_update = QDateTime::currentDateTime();
            
            emit DownloadProgressUpdated(operation_id, progress);
          });

  connect(reply, &QNetworkReply::finished,
          this, [this, operation_id, local_path, reply]() {
            if (reply->error() == QNetworkReply::NoError) {
              // Save file to local path
              QFile file(local_path);
              if (file.open(QIODevice::WriteOnly)) {
                file.write(reply->readAll());
                file.close();
                
                OnTransferCompleted(operation_id);
                emit FileDownloaded(operation_id, local_path);
                qInfo() << "File downloaded successfully:" << local_path;
              } else {
                OnTransferFailed(operation_id, "Failed to write local file");
              }
            } else {
              OnTransferFailed(operation_id, reply->errorString());
            }
            
            {
              QMutexLocker locker(&operations_mutex_);
              active_downloads_.remove(operation_id);
            }
            reply->deleteLater();
          });

  qInfo() << "Started download:" << operation_id << "from" << bucket << "/" << object_key;
  return operation_id;
}

QString MinIOClient::GetPresignedUrl(const QString& bucket, const QString& object_key,
                                   int expiry_seconds)
{
  QString cache_key = QString("%1/%2").arg(bucket, object_key);
  
  // Check cache first
  QString cached_url = GetCachedPresignedUrl(cache_key);
  if (!cached_url.isEmpty()) {
    QMutexLocker locker(&stats_mutex_);
    stats_.cache_hits++;
    return cached_url;
  }
  
  // Generate new presigned URL
  QString presigned_url = CreatePresignedUrl("GET", bucket, object_key, expiry_seconds);
  
  if (!presigned_url.isEmpty()) {
    CachePresignedUrl(cache_key, presigned_url, expiry_seconds);
    QMutexLocker locker(&stats_mutex_);
    stats_.cache_misses++;
  }
  
  return presigned_url;
}

VideoMetadata MinIOClient::GetVideoMetadata(const QString& file_id) const
{
  QMutexLocker locker(&cache_mutex_);
  
  if (video_cache_.contains(file_id)) {
    return video_cache_[file_id];
  }
  
  return VideoMetadata();
}

QList<VideoMetadata> MinIOClient::ListVideos(const QString& bucket, const QString& prefix,
                                            int max_results) const
{
  QList<VideoMetadata> videos;
  
  // Create list objects request
  QNetworkRequest request = CreateRequest("GET", bucket, QString());
  
  QUrl url = request.url();
  QUrlQuery query;
  if (!prefix.isEmpty()) {
    query.addQueryItem("prefix", prefix);
  }
  query.addQueryItem("max-keys", QString::number(max_results));
  url.setQuery(query);
  request.setUrl(url);
  
  // This would be a synchronous request in practice - using async pattern here
  QNetworkReply* reply = network_manager_->get(request);
  
  // Wait for completion (simplified for example)
  QEventLoop loop;
  connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
  loop.exec();
  
  if (reply->error() == QNetworkReply::NoError) {
    // Parse XML response and populate videos list
    // Implementation would parse MinIO's ListObjects XML response
    qInfo() << "Listed" << videos.size() << "videos from bucket:" << bucket;
  } else {
    qWarning() << "Failed to list videos:" << reply->errorString();
  }
  
  reply->deleteLater();
  return videos;
}

QList<FabricatorResult> MinIOClient::ListFabricatorResults(const QString& video_file_id) const
{
  QMutexLocker locker(&cache_mutex_);
  
  if (fabricator_cache_.contains(video_file_id)) {
    return fabricator_cache_[video_file_id];
  }
  
  return QList<FabricatorResult>();
}

FabricatorResult MinIOClient::GetFabricatorResultAt(const QString& video_file_id, 
                                                   qint64 video_timestamp) const
{
  QList<FabricatorResult> results = ListFabricatorResults(video_file_id);
  
  // Find closest result to timestamp
  FabricatorResult closest_result;
  qint64 min_diff = LLONG_MAX;
  
  for (const FabricatorResult& result : results) {
    qint64 diff = qAbs(result.video_timestamp - video_timestamp);
    if (diff < min_diff) {
      min_diff = diff;
      closest_result = result;
    }
  }
  
  // Only return if within 5 seconds
  if (min_diff < 5000) {
    return closest_result;
  }
  
  return FabricatorResult();
}

bool MinIOClient::DeleteFile(const QString& bucket, const QString& object_key)
{
  if (!is_connected_) {
    return false;
  }
  
  QNetworkRequest request = CreateRequest("DELETE", bucket, object_key);
  QNetworkReply* reply = network_manager_->deleteResource(request);
  
  // Wait for completion
  QEventLoop loop;
  connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
  loop.exec();
  
  bool success = (reply->error() == QNetworkReply::NoError);
  
  if (success) {
    qInfo() << "File deleted:" << bucket << "/" << object_key;
  } else {
    qWarning() << "Failed to delete file:" << reply->errorString();
  }
  
  reply->deleteLater();
  return success;
}

bool MinIOClient::FileExists(const QString& bucket, const QString& object_key) const
{
  if (!is_connected_) {
    return false;
  }
  
  QNetworkRequest request = CreateRequest("HEAD", bucket, object_key);
  QNetworkReply* reply = network_manager_->head(request);
  
  // Wait for completion
  QEventLoop loop;
  connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
  loop.exec();
  
  bool exists = (reply->error() == QNetworkReply::NoError);
  reply->deleteLater();
  
  return exists;
}

qint64 MinIOClient::GetFileSize(const QString& bucket, const QString& object_key) const
{
  if (!is_connected_) {
    return -1;
  }
  
  QNetworkRequest request = CreateRequest("HEAD", bucket, object_key);
  QNetworkReply* reply = network_manager_->head(request);
  
  // Wait for completion
  QEventLoop loop;
  connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
  loop.exec();
  
  qint64 size = -1;
  if (reply->error() == QNetworkReply::NoError) {
    size = reply->header(QNetworkRequest::ContentLengthHeader).toLongLong();
  }
  
  reply->deleteLater();
  return size;
}

bool MinIOClient::CreateBucket(const QString& bucket_name)
{
  if (!MinIOUtils::IsValidBucketName(bucket_name)) {
    qWarning() << "Invalid bucket name:" << bucket_name;
    return false;
  }
  
  QNetworkRequest request = CreateRequest("PUT", bucket_name, QString());
  QNetworkReply* reply = network_manager_->put(request, QByteArray());
  
  // Wait for completion
  QEventLoop loop;
  connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
  loop.exec();
  
  bool success = (reply->error() == QNetworkReply::NoError || 
                 reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 409); // Already exists
  
  if (success) {
    qInfo() << "Bucket ready:" << bucket_name;
  } else {
    qWarning() << "Failed to create bucket:" << reply->errorString();
  }
  
  reply->deleteLater();
  return success;
}

QJsonObject MinIOClient::GetStatistics() const
{
  QMutexLocker locker(&stats_mutex_);
  
  QJsonObject stats;
  stats["files_uploaded"] = static_cast<qint64>(stats_.files_uploaded);
  stats["files_downloaded"] = static_cast<qint64>(stats_.files_downloaded);
  stats["bytes_uploaded"] = static_cast<qint64>(stats_.bytes_uploaded);
  stats["bytes_downloaded"] = static_cast<qint64>(stats_.bytes_downloaded);
  stats["upload_failures"] = static_cast<qint64>(stats_.upload_failures);
  stats["download_failures"] = static_cast<qint64>(stats_.download_failures);
  stats["cache_hits"] = static_cast<qint64>(stats_.cache_hits);
  stats["cache_misses"] = static_cast<qint64>(stats_.cache_misses);
  stats["cache_hit_ratio"] = static_cast<double>(stats_.cache_hits) / 
                            qMax(1LL, stats_.cache_hits + stats_.cache_misses);
  stats["average_upload_speed"] = stats_.average_upload_speed;
  stats["average_download_speed"] = stats_.average_download_speed;
  stats["uptime_seconds"] = stats_.start_time.secsTo(QDateTime::currentDateTime());
  stats["is_connected"] = is_connected_;
  stats["active_uploads"] = active_uploads_.size();
  stats["active_downloads"] = active_downloads_.size();
  
  return stats;
}

void MinIOClient::SetMultipartUpload(bool enabled, qint64 part_size)
{
  multipart_enabled_ = enabled;
  multipart_part_size_ = part_size;
  
  qInfo() << "Multipart upload" << (enabled ? "enabled" : "disabled")
          << "part size:" << MinIOUtils::FormatFileSize(part_size);
}

void MinIOClient::SetTimeouts(int connect_timeout_ms, int transfer_timeout_ms)
{
  connect_timeout_ms_ = connect_timeout_ms;
  transfer_timeout_ms_ = transfer_timeout_ms;
  
  if (network_manager_) {
    network_manager_->setTransferTimeout(transfer_timeout_ms);
  }
  
  qInfo() << "Timeouts updated - connect:" << connect_timeout_ms 
          << "ms, transfer:" << transfer_timeout_ms << "ms";
}

void MinIOClient::SetVideoAnalysisMode(bool enabled)
{
  video_analysis_enabled_ = enabled;
  qInfo() << "Video analysis mode" << (enabled ? "enabled" : "disabled");
}

void MinIOClient::CancelAllTransfers()
{
  QMutexLocker locker(&operations_mutex_);
  
  // Cancel uploads
  for (MinIOUploadOperation* upload : active_uploads_) {
    upload->Cancel();
  }
  
  // Cancel downloads
  for (QNetworkReply* reply : active_downloads_) {
    reply->abort();
  }
  
  qInfo() << "Cancelled" << active_uploads_.size() << "uploads and" 
          << active_downloads_.size() << "downloads";
}

void MinIOClient::OnTransferProgress(const QString& operation_id, const TransferProgress& progress)
{
  if (progress.status == "uploading") {
    emit UploadProgressUpdated(operation_id, progress);
  } else if (progress.status == "downloading") {
    emit DownloadProgressUpdated(operation_id, progress);
  }
}

void MinIOClient::OnTransferCompleted(const QString& operation_id)
{
  QMutexLocker locker(&operations_mutex_);
  
  if (active_uploads_.contains(operation_id)) {
    active_uploads_[operation_id]->deleteLater();
    active_uploads_.remove(operation_id);
    UpdateStatistics("upload", 0, true);
  }
  
  completed_operations_.enqueue(operation_id);
}

void MinIOClient::OnTransferFailed(const QString& operation_id, const QString& error)
{
  QMutexLocker locker(&operations_mutex_);
  
  if (active_uploads_.contains(operation_id)) {
    UpdateStatistics("upload", 0, false);
    emit UploadFailed(operation_id, error);
  } else if (active_downloads_.contains(operation_id)) {
    UpdateStatistics("download", 0, false);
    emit DownloadFailed(operation_id, error);
  }
  
  failed_operations_.enqueue(operation_id);
}

void MinIOClient::SetupNetworking()
{
  network_manager_ = new QNetworkAccessManager(this);
  network_manager_->setTransferTimeout(transfer_timeout_ms_);
  
  connect(network_manager_, &QNetworkAccessManager::finished,
          this, &MinIOClient::OnNetworkReplyFinished);
}

void MinIOClient::SetupTimers()
{
  // Statistics timer
  stats_timer_ = new QTimer(this);
  stats_timer_->setInterval(10000); // 10 seconds
  connect(stats_timer_, &QTimer::timeout, this, &MinIOClient::OnStatsTimer);
  
  // Cleanup timer
  cleanup_timer_ = new QTimer(this);
  cleanup_timer_->setInterval(60000); // 1 minute
  connect(cleanup_timer_, &QTimer::timeout, this, &MinIOClient::CleanupCompletedOperations);
  cleanup_timer_->start();
}

QString MinIOClient::GenerateOperationId() const
{
  return QUuid::createUuid().toString().remove('{').remove('}');
}

QNetworkRequest MinIOClient::CreateRequest(const QString& method, const QString& bucket,
                                         const QString& object_key, const QJsonObject& headers) const
{
  QString scheme = use_ssl_ ? "https" : "http";
  QString url_string = QString("%1://%2/%3").arg(scheme, endpoint_, bucket);
  
  if (!object_key.isEmpty()) {
    url_string += "/" + object_key;
  }
  
  QNetworkRequest request;
  request.setUrl(QUrl(url_string));
  
  // Add authentication headers
  QString date = QDateTime::currentDateTimeUtc().toString("ddd, dd MMM yyyy hh:mm:ss 'GMT'");
  request.setRawHeader("Date", date.toUtf8());
  request.setRawHeader("Host", endpoint_.toUtf8());
  
  // Create signature
  QString signature = CreateSignature(method, bucket, object_key, date, headers);
  QString auth_header = QString("AWS %1:%2").arg(access_key_, signature);
  request.setRawHeader("Authorization", auth_header.toUtf8());
  
  // Add custom headers
  for (auto it = headers.constBegin(); it != headers.constEnd(); ++it) {
    request.setRawHeader(it.key().toUtf8(), it.value().toString().toUtf8());
  }
  
  return request;
}

QString MinIOClient::CreateSignature(const QString& method, const QString& bucket, 
                                    const QString& object_key, const QString& date,
                                    const QJsonObject& headers) const
{
  // Simplified signature creation - in practice would use proper AWS signature v4
  QString string_to_sign = QString("%1\n\n\n%2\n/%3/%4")
                          .arg(method, date, bucket, object_key);
  
  QByteArray signature = QCryptographicHash::hash(
    (secret_key_ + string_to_sign).toUtf8(), 
    QCryptographicHash::Sha256
  ).toBase64();
  
  return QString::fromUtf8(signature);
}

QString MinIOClient::CreatePresignedUrl(const QString& method, const QString& bucket,
                                       const QString& object_key, int expiry_seconds,
                                       const QJsonObject& query_params) const
{
  QString scheme = use_ssl_ ? "https" : "http";
  qint64 expires = QDateTime::currentSecsSinceEpoch() + expiry_seconds;
  
  QUrl url(QString("%1://%2/%3/%4").arg(scheme, endpoint_, bucket, object_key));
  QUrlQuery query;
  
  query.addQueryItem("AWSAccessKeyId", access_key_);
  query.addQueryItem("Expires", QString::number(expires));
  query.addQueryItem("Signature", CreateSignature(method, bucket, object_key, QString::number(expires)));
  
  // Add custom query parameters
  for (auto it = query_params.constBegin(); it != query_params.constEnd(); ++it) {
    query.addQueryItem(it.key(), it.value().toString());
  }
  
  url.setQuery(query);
  return url.toString();
}

void MinIOClient::ProcessVideoFile(const QString& file_path, VideoMetadata& metadata)
{
  ExtractVideoMetadata(file_path, metadata);
  
  if (video_analysis_enabled_) {
    // Generate thumbnails and additional analysis
    // This would be handled by VideoAnalysisWorker in practice
    qInfo() << "Processing video analysis for:" << metadata.original_filename;
  }
}

void MinIOClient::ExtractVideoMetadata(const QString& file_path, VideoMetadata& metadata)
{
  // Use FFmpeg to extract video metadata
  AVFormatContext* format_ctx = nullptr;
  
  if (avformat_open_input(&format_ctx, file_path.toUtf8().constData(), nullptr, nullptr) != 0) {
    qWarning() << "Could not open video file for metadata extraction:" << file_path;
    return;
  }
  
  if (avformat_find_stream_info(format_ctx, nullptr) < 0) {
    qWarning() << "Could not find stream information";
    avformat_close_input(&format_ctx);
    return;
  }
  
  // Extract metadata
  metadata.duration_ms = static_cast<int>(format_ctx->duration * 1000 / AV_TIME_BASE);
  
  // Find video and audio streams
  for (unsigned int i = 0; i < format_ctx->nb_streams; i++) {
    AVStream* stream = format_ctx->streams[i];
    AVCodecParameters* codecpar = stream->codecpar;
    
    if (codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
      metadata.width = codecpar->width;
      metadata.height = codecpar->height;
      metadata.bitrate = static_cast<int>(codecpar->bit_rate);
      metadata.frame_rate = av_q2d(stream->avg_frame_rate);
      
      const AVCodec* codec = avcodec_find_decoder(codecpar->codec_id);
      if (codec) {
        metadata.video_codec = QString::fromUtf8(codec->name);
      }
    } else if (codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
      const AVCodec* codec = avcodec_find_decoder(codecpar->codec_id);
      if (codec) {
        metadata.audio_codec = QString::fromUtf8(codec->name);
      }
    }
  }
  
  avformat_close_input(&format_ctx);
  
  qInfo() << "Extracted video metadata:" << metadata.original_filename
          << "duration:" << metadata.duration_ms << "ms"
          << "resolution:" << metadata.width << "x" << metadata.height;
}

void MinIOClient::UpdateVideoCache(const VideoMetadata& metadata)
{
  QMutexLocker locker(&cache_mutex_);
  video_cache_[metadata.file_id] = metadata;
}

void MinIOClient::UpdateFabricatorCache(const FabricatorResult& result)
{
  QMutexLocker locker(&cache_mutex_);
  
  if (!fabricator_cache_.contains(result.video_file_id)) {
    fabricator_cache_[result.video_file_id] = QList<FabricatorResult>();
  }
  
  fabricator_cache_[result.video_file_id].append(result);
  
  // Sort by timestamp
  std::sort(fabricator_cache_[result.video_file_id].begin(),
           fabricator_cache_[result.video_file_id].end(),
           [](const FabricatorResult& a, const FabricatorResult& b) {
             return a.video_timestamp < b.video_timestamp;
           });
}

void MinIOClient::CachePresignedUrl(const QString& cache_key, const QString& url, int expiry_seconds)
{
  QMutexLocker locker(&cache_mutex_);
  
  QDateTime expiry_time = QDateTime::currentDateTime().addSecs(expiry_seconds - 60); // 1 minute buffer
  presigned_url_cache_[cache_key] = qMakePair(url, expiry_time);
}

QString MinIOClient::GetCachedPresignedUrl(const QString& cache_key) const
{
  QMutexLocker locker(&cache_mutex_);
  
  if (presigned_url_cache_.contains(cache_key)) {
    QPair<QString, QDateTime> cached = presigned_url_cache_[cache_key];
    if (cached.second > QDateTime::currentDateTime()) {
      return cached.first;
    } else {
      // URL expired, remove from cache
      presigned_url_cache_.remove(cache_key);
    }
  }
  
  return QString();
}

bool MinIOClient::ValidateConfiguration() const
{
  if (endpoint_.isEmpty() || access_key_.isEmpty() || secret_key_.isEmpty()) {
    qCritical() << "MinIO configuration incomplete";
    return false;
  }
  
  if (!endpoint_.contains(':')) {
    qWarning() << "Endpoint should include port (e.g., localhost:9000)";
  }
  
  return true;
}

void MinIOClient::HandleConnectionFailure(const QString& error)
{
  is_connected_ = false;
  emit ConnectionError(error);
  qWarning() << "MinIO connection failure:" << error;
}

void MinIOClient::UpdateStatistics(const QString& operation_type, qint64 bytes, bool success)
{
  QMutexLocker locker(&stats_mutex_);
  
  if (operation_type == "upload") {
    if (success) {
      stats_.files_uploaded++;
      stats_.bytes_uploaded += bytes;
    } else {
      stats_.upload_failures++;
    }
  } else if (operation_type == "download") {
    if (success) {
      stats_.files_downloaded++;
      stats_.bytes_downloaded += bytes;
    } else {
      stats_.download_failures++;
    }
  }
}

void MinIOClient::OnNetworkReplyFinished()
{
  // Handle generic network reply completion
  QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
  if (reply) {
    if (reply->error() != QNetworkReply::NoError) {
      qWarning() << "Network request failed:" << reply->errorString();
    }
    reply->deleteLater();
  }
}

void MinIOClient::OnStatsTimer()
{
  emit StatisticsUpdated(GetStatistics());
}

void MinIOClient::CleanupCompletedOperations()
{
  QMutexLocker locker(&operations_mutex_);
  
  // Clean up completed operations
  while (!completed_operations_.isEmpty()) {
    completed_operations_.dequeue();
  }
  
  // Clean up old failed operations
  while (failed_operations_.size() > 100) {
    failed_operations_.dequeue();
  }
}

// MinIOUtils namespace implementation
namespace MinIOUtils {

QString GenerateVideoObjectKey(const QString& original_filename, const QDateTime& timestamp)
{
  QFileInfo file_info(original_filename);
  QString date_path = timestamp.toString("yyyy/MM/dd");
  QString unique_id = QUuid::createUuid().toString().remove('{').remove('}').left(8);
  
  return QString("videos/%1/%2_%3.%4")
         .arg(date_path, unique_id, file_info.baseName(), file_info.suffix().toLower());
}

QString GenerateFabricatorObjectKey(const QString& video_file_id, const QString& result_type, 
                                   qint64 video_timestamp)
{
  QString timestamp_str = QString::number(video_timestamp);
  QString unique_id = QUuid::createUuid().toString().remove('{').remove('}').left(8);
  
  return QString("fabricator/%1/%2/%3_%4.json")
         .arg(video_file_id, result_type, timestamp_str, unique_id);
}

bool IsValidBucketName(const QString& bucket_name)
{
  if (bucket_name.length() < 3 || bucket_name.length() > 63) {
    return false;
  }
  
  QRegularExpression regex("^[a-z0-9][a-z0-9.-]*[a-z0-9]$");
  return regex.match(bucket_name).hasMatch();
}

QString GetMimeType(const QString& file_path)
{
  QMimeDatabase mime_db;
  QMimeType mime_type = mime_db.mimeTypeForFile(file_path);
  return mime_type.name();
}

QString FormatFileSize(qint64 bytes)
{
  const QStringList units = {"B", "KB", "MB", "GB", "TB"};
  int unit_index = 0;
  double size = static_cast<double>(bytes);
  
  while (size >= 1024.0 && unit_index < units.size() - 1) {
    size /= 1024.0;
    unit_index++;
  }
  
  return QString("%1 %2").arg(QString::number(size, 'f', 2), units[unit_index]);
}

QString CalculateETag(const QString& file_path)
{
  QFile file(file_path);
  if (!file.open(QIODevice::ReadOnly)) {
    return QString();
  }
  
  QCryptographicHash hash(QCryptographicHash::Md5);
  hash.addData(&file);
  
  return QString::fromUtf8(hash.result().toHex());
}

bool IsValidVideoFormat(const QString& file_path)
{
  QFileInfo file_info(file_path);
  QString suffix = file_info.suffix().toLower();
  
  QStringList valid_formats = {"mp4", "avi", "mov", "mkv", "wmv", "flv", "webm", "m4v"};
  return valid_formats.contains(suffix);
}

QJsonObject GetStreamingParams(const VideoMetadata& metadata, const QString& quality)
{
  QJsonObject params;
  
  if (quality == "auto") {
    // Auto-select based on resolution
    if (metadata.width >= 1920) {
      params["quality"] = "1080p";
      params["bitrate"] = "5000k";
    } else if (metadata.width >= 1280) {
      params["quality"] = "720p";
      params["bitrate"] = "2500k";
    } else {
      params["quality"] = "480p";
      params["bitrate"] = "1000k";
    }
  } else {
    params["quality"] = quality;
  }
  
  params["format"] = "mp4";
  params["codec"] = "h264";
  params["original_width"] = metadata.width;
  params["original_height"] = metadata.height;
  params["original_bitrate"] = metadata.bitrate;
  
  return params;
}

QByteArray ExtractVideoFrame(const QString& file_path, qint64 timestamp_ms)
{
  // Simplified frame extraction using FFmpeg
  AVFormatContext* format_ctx = nullptr;
  AVCodecContext* codec_ctx = nullptr;
  AVFrame* frame = nullptr;
  AVPacket* packet = nullptr;
  QByteArray frame_data;
  
  // This is a simplified implementation
  // Full implementation would seek to timestamp and extract frame
  
  qInfo() << "Extracting video frame at:" << timestamp_ms << "ms from:" << file_path;
  return frame_data;
}

} // namespace MinIOUtils

} // namespace olive
