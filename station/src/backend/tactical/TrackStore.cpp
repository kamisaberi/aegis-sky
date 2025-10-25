#include "TrackStore.h"

namespace aegis::station::tactical {

    TrackStore::TrackStore(QObject *parent) : QAbstractListModel(parent) {}

    int TrackStore::rowCount(const QModelIndex &parent) const {
        return m_tracks.count();
    }

    QVariant TrackStore::data(const QModelIndex &index, int role) const {
        if (!index.isValid() || index.row() >= m_tracks.count())
            return QVariant();

        const TrackObject &track = m_tracks[index.row()];

        switch (role) {
            case IdRole: return track.id;
            case AzimuthRole: return track.azimuth;
            case ElevationRole: return track.elevation;
            case IsThreatRole: return track.is_threat;
            case LabelRole: return track.label;
            default: return QVariant();
        }
    }

    QHash<int, QByteArray> TrackStore::roleNames() const {
        QHash<int, QByteArray> roles;
        roles[IdRole] = "trackId";
        roles[AzimuthRole] = "azimuth";
        roles[ElevationRole] = "elevation";
        roles[IsThreatRole] = "isThreat";
        roles[LabelRole] = "label";
        return roles;
    }

    void TrackStore::updateTracks(const QList<TrackObject>& newTracks) {
        // For simplicity, we do a full reset. 
        // In prod, use diffing (layoutChanged) for better performance with 1000+ items.
        beginResetModel();
        m_tracks = newTracks.toVector(); // Convert QList to QVector
        endResetModel();
    }

    void TrackStore::clear() {
        beginResetModel();
        m_tracks.clear();
        endResetModel();
    }
}