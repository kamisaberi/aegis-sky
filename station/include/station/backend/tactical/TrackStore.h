#pragma once
#include <QAbstractListModel>
#include <QVector>
#include "backend/comms/StationProtocol.h" // For TrackData struct

namespace aegis::station::tactical {

    struct TrackObject {
        uint32_t id;
        float azimuth;   // Radians relative to camera center
        float elevation; // Radians relative to camera center
        bool is_threat;
        QString label;   // e.g., "DRONE-01"
    };

    class TrackStore : public QAbstractListModel {
        Q_OBJECT
    public:
        enum TrackRoles {
            IdRole = Qt::UserRole + 1,
            AzimuthRole,
            ElevationRole,
            IsThreatRole,
            LabelRole
        };

        explicit TrackStore(QObject *parent = nullptr);

        // QAbstractListModel Interface
        int rowCount(const QModelIndex &parent = QModelIndex()) const override;
        QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
        QHash<int, QByteArray> roleNames() const override;

    public slots:
        // Called by CoreClient when packet arrives
        void updateTracks(const QList<TrackObject>& newTracks);
        void clear();

    private:
        QVector<TrackObject> m_tracks;
    };
}