/***
  Apache-Cleats Sports Editor
  Copyright (C) 2024 AnalyzeMyTeam

  MinIO Client for Video Storage Integration
  High-performance video streaming and Dynamic Fabricator result storage
***/

#ifndef MINIOCLIENT_H
#define MINIOCLIENT_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QMutex>
#include <QQueue>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDateTime>
#include <QFileInfo>
#include <QIODevice>
#include <QByteArray>
#include <QUrl>
#include <QUrlQuery>
#include <QCryptographicHash>
#include <QThread>
#include <QProgressBar>

namespace olive {

/**
 * @brief Video file metadata structure
 */
struct VideoMetadata {
  QString file_id;
  QString original_filename;
  QString bucket_name;
  QString object_key;
  qint64 file_size;
  QString content_type;
  QDateTime upload_timestamp;
  QDateTime last_modified;
  QString etag;
  QJsonObject custom_metadata;
  QString video_codec;
  QString audio_codec;
  int duration_ms;
  int width;
  int height;
  double frame_rate;
  int bitrate;
  
  VideoMetadata() : file_size(0), duration_ms(0), width(0), height(0), 
                    frame_rate(0.0), bitrate(0) {}
};

/**
 * @brief Dynamic Fabricator result metadata
 */
struct FabricatorResult {
  QString result_id;
  QString video_file_id;
  QString result_type; // "diagram", "analysis", "overlay"
  QString bucket_name;
  QString object_key;
  qint64 file_size;
  QDateTime generation_timestamp;
  qint64 video_timestamp; // Associated video timestamp
  QJsonObject processing_metadata;
  QString formation_id;
  QString triangle_call;
  double confidence;
  
  FabricatorResult() : file_size(0), generation_timestamp(QDateTime::currentDateTime()),
                       video_timestamp(0), confidence(0.0) {}
};

/**
 * @brief Upload/Download progress tracking
 */
struct TransferProgress {
  QString operation_id;
  QString file_path;
  qint64 bytes_transferred;
  qint64 total_bytes;
  double percentage;
  QString status; // "pending", "uploading", "downloading", "completed", "failed"
  QDateTime start_time;
  QDateTime last_update;
  QString error_message;
  
  TransferProgress() : bytes_transferred(0), total_bytes(0), percentage(0.0) {}
};

/**
 * @brief MinIO upload operation
 */
class MinIOUploadOperation : public QObject
{
  Q_OBJECT

public:
  explicit MinIOUploadOperation(const QString& operation_id, const QString& file_path,
                               const QString& bucket, const QString& object_key,
                               QObject* parent = nullptr);

  QString GetOperationId() const { return operation_id_; }
  QString GetFilePath() const { return file_path_; }
  QString GetBucketName() const { return bucket_name_; }
  QString GetObjectKey() const { return object_key_; }
  TransferProgress GetProgress() const { return progress_; }

public slots:
  void Start();
  void Cancel();
  void Retry();

signals:
  void ProgressUpdated(const TransferProgress& progress);
  void Completed(const VideoMetadata& metadata);
  void Failed(const QString& error);

private slots:
  void OnUploadProgress(qint64 bytes_sent, qint64 bytes_total);
  void OnUploadFinished();

private:
  QString operation_id_;
  QString file_path_;
  QString bucket_name_;
  QString object_key_;
  TransferProgress progress_;
  QNetworkReply* current_reply_;
  QNetworkAccessManager* network_manager_;
  int retry_count_;
  static const int MAX_RETRIES = 3;
};

/**
 * @brief High-performance MinIO client for video storage
 */
class MinIOClient : public QObject
{
  Q_OBJECT

public:
  explicit MinIOClient(QObject* parent = nullptr);
  virtual ~MinIOClient();

  /**
   * @brief Initialize MinIO connection
   */
  bool Initialize(const QString& endpoint, const QString& access_key,
                  const QString& secret_key, bool use_ssl = true);

  /**
   * @brief Shutdown client gracefully
   */
  void Shutdown();

  /**
   * @brief Upload video file asynchronously
   */
  QString UploadVideoFile(const QString& file_path, const QString& bucket = "videos",
                         const QJsonObject& metadata = QJsonObject());

  /**
   * @brief Upload Dynamic Fabricator result
   */
  QString UploadFabricatorResult(const QString& file_path, const QString& video_file_id,
                                const QString& result_type, qint64 video_timestamp,
                                const QJsonObject& processing_metadata = QJsonObject());

  /**
   * @brief Download file with progress tracking
   */
  QString DownloadFile(const QString& bucket, const QString& object_key,
                      const QString& local_path);

  /**
   * @brief Get presigned URL for video streaming
   */
  QString GetPresignedUrl(const QString& bucket, const QString& object_key,
                         int expiry_seconds = 3600);

  /**
   * @brief Get video metadata
   */
  VideoMetadata GetVideoMetadata(const QString& file_id) const;

  /**
   * @brief List videos in bucket with filtering
   */
  QList<VideoMetadata> ListVideos(const QString& bucket = "videos",
                                 const QString& prefix = QString(),
                                 int max_results = 1000) const;

  /**
   * @brief List Fabricator results for video
   */
  QList<FabricatorResult> ListFabricatorResults(const QString& video_file_id) const;

  /**
   * @brief Get Fabricator result at specific timestamp
   */
  FabricatorResult GetFabricatorResultAt(const QString& video_file_id, 
                                        qint64 video_timestamp) const;

  /**
   * @brief Delete file from storage
   */
  bool DeleteFile(const QString& bucket, const QString& object_key);

  /**
   * @brief Check if file exists
   */
  bool FileExists(const QString& bucket, const QString& object_key) const;

  /**
   * @brief Get file size
   */
  qint64 GetFileSize(const QString& bucket, const QString& object_key) const;

  /**
   * @brief Create bucket if not exists
   */
  bool CreateBucket(const QString& bucket_name);

  /**
   * @brief Set bucket policy for public access
   */
  bool SetBucketPolicy(const QString& bucket_name, const QString& policy);

  /**
   * @brief Get connection status
   */
  bool IsConnected() const { return is_connected_; }

  /**
   * @brief Get client statistics
   */
  QJsonObject GetStatistics() const;

  /**
   * @brief Enable/disable multipart uploads
   */
  void SetMultipartUpload(bool enabled, qint64 part_size = 5 * 1024 * 1024); // 5MB default

  /**
   * @brief Set upload/download timeouts
   */
  void SetTimeouts(int connect_timeout_ms = 10000, int transfer_timeout_ms = 300000);

  /**
   * @brief Enable video analysis integration
   */
  void SetVideoAnalysisMode(bool enabled);

public slots:
  /**
   * @brief Handle upload/download events
   */
  void OnTransferProgress(const QString& operation_id, const TransferProgress& progress);
  void OnTransferCompleted(const QString& operation_id);
  void OnTransferFailed(const QString& operation_id, const QString& error);

  /**
   * @brief Cancel all active transfers
   */
  void CancelAllTransfers();

  /**
   * @brief Retry failed transfers
   */
  void RetryFailedTransfers();

signals:
  /**
   * @brief Transfer progress signals
   */
  void UploadProgressUpdated(const QString& operation_id, const TransferProgress& progress);
  void DownloadProgressUpdated(const QString& operation_id, const TransferProgress& progress);
  void VideoUploaded(const VideoMetadata& metadata);
  void FabricatorResultUploaded(const FabricatorResult& result);
  void FileDownloaded(const QString& operation_id, const QString& local_path);

  /**
   * @brief Error signals
   */
  void UploadFailed(const QString& operation_id, const QString& error);
  void DownloadFailed(const QString& operation_id, const QString& error);
  void ConnectionError(const QString& error);

  /**
   * @brief Status signals
   */
  void Connected();
  void Disconnected();
  void StatisticsUpdated(const QJsonObject& stats);

private slots:
  void OnNetworkReplyFinished();
  void OnStatsTimer();
  void CleanupCompletedOperations();

private:
  void SetupNetworking();
  void SetupTimers();
  QString GenerateOperationId() const;
  QNetworkRequest CreateRequest(const QString& method, const QString& bucket,
                               const QString& object_key, const QJsonObject& headers = QJsonObject()) const;
  QString CreateSignature(const QString& method, const QString& bucket, const QString& object_key,
                         const QString& date, const QJsonObject& headers = QJsonObject()) const;
  QString CreatePresignedUrl(const QString& method, const QString& bucket, const QString& object_key,
                            int expiry_seconds, const QJsonObject& query_params = QJsonObject()) const;
  
  void ProcessVideoFile(const QString& file_path, VideoMetadata& metadata);
  void ExtractVideoMetadata(const QString& file_path, VideoMetadata& metadata);
  void UpdateVideoCache(const VideoMetadata& metadata);
  void UpdateFabricatorCache(const FabricatorResult& result);
  void CachePresignedUrl(const QString& cache_key, const QString& url, int expiry_seconds);
  QString GetCachedPresignedUrl(const QString& cache_key) const;
  
  bool ValidateConfiguration() const;
  void HandleConnectionFailure(const QString& error);
  void UpdateStatistics(const QString& operation_type, qint64 bytes, bool success);

  // Configuration
  QString endpoint_;
  QString access_key_;
  QString secret_key_;
  bool use_ssl_;
  bool is_connected_;
  bool is_initialized_;
  
  // Upload/Download settings
  bool multipart_enabled_;
  qint64 multipart_part_size_;
  int connect_timeout_ms_;
  int transfer_timeout_ms_;
  bool video_analysis_enabled_;
  
  // Network
  QNetworkAccessManager* network_manager_;
  QTimer* stats_timer_;
  QTimer* cleanup_timer_;
  
  // Operation tracking
  mutable QMutex operations_mutex_;
  QMap<QString, MinIOUploadOperation*> active_uploads_;
  QMap<QString, QNetworkReply*> active_downloads_;
  QQueue<QString> completed_operations_;
  QQueue<QString> failed_operations_;
  
  // Caching
  mutable QMutex cache_mutex_;
  QMap<QString, VideoMetadata> video_cache_;
  QMap<QString, QList<FabricatorResult>> fabricator_cache_;
  QMap<QString, QPair<QString, QDateTime>> presigned_url_cache_; // key -> (url, expiry)
  
  // Statistics
  mutable QMutex stats_mutex_;
  struct Statistics {
    qint64 files_uploaded;
    qint64 files_downloaded;
    qint64 bytes_uploaded;
    qint64 bytes_downloaded;
    qint64 upload_failures;
    qint64 download_failures;
    qint64 cache_hits;
    qint64 cache_misses;
    double average_upload_speed; // bytes per second
    double average_download_speed;
    QDateTime start_time;
    
    Statistics() : files_uploaded(0), files_downloaded(0), bytes_uploaded(0),
                   bytes_downloaded(0), upload_failures(0), download_failures(0),
                   cache_hits(0), cache_misses(0), average_upload_speed(0.0),
                   average_download_speed(0.0), start_time(QDateTime::currentDateTime()) {}
  } stats_;
};

/**
 * @brief Worker thread for video analysis
 */
class VideoAnalysisWorker : public QObject
{
  Q_OBJECT

public:
  explicit VideoAnalysisWorker(QObject* parent = nullptr);

public slots:
  void AnalyzeVideo(const QString& file_path, const QString& file_id);

signals:
  void AnalysisCompleted(const QString& file_id, const QJsonObject& analysis);
  void AnalysisFailed(const QString& file_id, const QString& error);

private:
  void ExtractVideoInfo(const QString& file_path, QJsonObject& info);
  void GenerateThumbnails(const QString& file_path, const QString& file_id);
  void ExtractAudioWaveform(const QString& file_path, QJsonObject& waveform);
};

/**
 * @brief Utility functions for MinIO integration
 */
namespace MinIOUtils {

/**
 * @brief Generate unique object key for video
 */
QString GenerateVideoObjectKey(const QString& original_filename, const QDateTime& timestamp = QDateTime::currentDateTime());

/**
 * @brief Generate object key for Fabricator result
 */
QString GenerateFabricatorObjectKey(const QString& video_file_id, const QString& result_type, qint64 video_timestamp);

/**
 * @brief Validate bucket name format
 */
bool IsValidBucketName(const QString& bucket_name);

/**
 * @brief Get MIME type from file extension
 */
QString GetMimeType(const QString& file_path);

/**
 * @brief Format file size for display
 */
QString FormatFileSize(qint64 bytes);

/**
 * @brief Calculate ETag for file
 */
QString CalculateETag(const QString& file_path);

/**
 * @brief Extract video frame at timestamp
 */
QByteArray ExtractVideoFrame(const QString& file_path, qint64 timestamp_ms);

/**
 * @brief Validate video file format
 */
bool IsValidVideoFormat(const QString& file_path);

/**
 * @brief Get optimal streaming URL parameters
 */
QJsonObject GetStreamingParams(const VideoMetadata& metadata, const QString& quality = "auto");

} // namespace MinIOUtils

} // namespace olive

Q_DECLARE_METATYPE(olive::VideoMetadata)
Q_DECLARE_METATYPE(olive::FabricatorResult)
Q_DECLARE_METATYPE(olive::TransferProgress)

#endif // MINIOCLIENT_H
