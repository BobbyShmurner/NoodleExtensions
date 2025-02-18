#include "GlobalNamespace/BeatmapObjectCallbackController.hpp"
#include "GlobalNamespace/BeatmapObjectSpawnController.hpp"
#include "GlobalNamespace/BeatmapObjectSpawnMovementData.hpp"
#include "custom-json-data/shared/CustomBeatmapData.h"
#include "tracks/shared/Animation/PointDefinition.h"
#include "Animation/AnimationHelper.h"
#include "AssociatedData.h"
#include "NELogger.h"

using namespace AnimationHelper;
using namespace GlobalNamespace;
using namespace NEVector;
using namespace CustomJSONData;

// BeatmapObjectCallbackController.cpp
extern BeatmapObjectCallbackController *callbackController;

// Events.cpp
extern BeatmapObjectSpawnController *spawnController;

constexpr std::optional<Vector3> operator+(std::optional<Vector3> const& a, std::optional<Vector3> const& b) {
    if (!a && !b) {
        return std::nullopt;
    }

    Vector3 total = Vector3::zero();
    if (a) {
        total = total + *a;
    }

    if (b) {
        total = total + *b;
    }

    return total;
}

template<typename T>
constexpr std::optional<T> operator*(std::optional<T> const& a, std::optional<T> const& b) {
    if (a && b) {
        return *a * *b;
    } else if (a) {
        return a;
    } else if (b) {
        return b;
    } else {
        return std::nullopt;
    }
}

std::optional<NEVector::Vector3> AnimationHelper::GetDefinitePositionOffset(const AnimationObjectData& animationData, std::vector<Track *> const& tracks, float time) {
    PointDefinition *localDefinitePosition = animationData.definitePosition;

    std::optional<Vector3> pathDefinitePosition =
            localDefinitePosition ? std::optional(localDefinitePosition->Interpolate(time)) : std::nullopt;

    if (!pathDefinitePosition && !tracks.empty()) {
        if (tracks.size() == 1) {
            Track *track = tracks.front();
            pathDefinitePosition = getPathPropertyNullable<Vector3>(track, track->pathProperties.definitePosition.value, time);
        } else {
            pathDefinitePosition = MSumTrackPathProps(tracks, Vector3::zero(), time, definitePosition);
        }
    }

    if (!pathDefinitePosition)
        return std::nullopt;

    PointDefinition *position = animationData.position;
    std::optional<Vector3> pathPosition = position ? std::optional(position->Interpolate(time)) : std::nullopt;
    std::optional<Vector3> trackPosition;

    std::optional<Vector3> positionOffset;

    if (!tracks.empty()) {
        if (tracks.size() == 1) {
            Track *track = tracks.front();

            if (!pathPosition)
                pathPosition = getPathPropertyNullable<Vector3>(track, track->pathProperties.position.value, time);

            trackPosition = getPropertyNullable<Vector3>(track, track->properties.position);
        } else {
            trackPosition = MSumTrackProps(tracks, Vector3::zero(), position);

            if (!pathPosition)
                pathPosition = MSumTrackPathProps(tracks, Vector3::zero(), time, position);
        }

        positionOffset = pathPosition + trackPosition;
    } else {
        positionOffset = pathPosition;
    }

    std::optional<Vector3> definitePosition = positionOffset + pathDefinitePosition;
    if (definitePosition)
        definitePosition = definitePosition.value() *
                           spawnController->beatmapObjectSpawnMovementData->noteLinesDistance;

    return definitePosition;
}

ObjectOffset AnimationHelper::GetObjectOffset(const AnimationObjectData& animationData, std::vector<Track *> const& tracks, float time) {
    ObjectOffset offset;

    PointDefinition *position = animationData.position;
    PointDefinition *rotation = animationData.rotation;
    PointDefinition *scale = animationData.scale;
    PointDefinition *localRotation = animationData.localRotation;
    PointDefinition *dissolve = animationData.dissolve;
    PointDefinition *dissolveArrow = animationData.dissolveArrow;
    PointDefinition *cuttable = animationData.cuttable;
#define pathPropPointDef(type, name, interpolate) \
std::optional<type> path##name = (name) ? std::optional<type>((name)->interpolate(time)) : std::nullopt;

    pathPropPointDef(Vector3, position, Interpolate)
    pathPropPointDef(Quaternion, rotation, InterpolateQuaternion)
    pathPropPointDef(Vector3, scale, Interpolate)
    pathPropPointDef(Quaternion, localRotation, InterpolateQuaternion)
    pathPropPointDef(float, dissolve, InterpolateLinear)
    pathPropPointDef(float, dissolveArrow, InterpolateLinear)
    pathPropPointDef(float, cuttable, InterpolateLinear)


    if (!tracks.empty()) {
        if (tracks.size() == 1) {
            auto track = tracks.front();

#define singlePathProp(type, name, interpolate) \
if (!path##name)                                \
    path##name = getPathPropertyNullable<type>(track, track->pathProperties.name.value, time);

#define offsetProp(type, name, offsetName, op) \
offset.offsetName = path##name op getPropertyNullable<type>(track, track->properties.name);


            singlePathProp(Vector3, position, Interpolate)
            singlePathProp(Quaternion, rotation, InterpolateQuaternion)
            singlePathProp(Vector3, scale, Interpolate)
            singlePathProp(Quaternion, localRotation, InterpolateQuaternion)
            singlePathProp(float, dissolve, InterpolateLinear)
            singlePathProp(float, dissolveArrow, InterpolateLinear)
            singlePathProp(float, cuttable, InterpolateLinear)


            offsetProp(Vector3, position, positionOffset, +)
            offsetProp(Quaternion, rotation, rotationOffset, *)
            offsetProp(Vector3, scale, scaleOffset, *)
            offsetProp(Quaternion, localRotation, localRotationOffset, *)
            offsetProp(float, dissolve, dissolve, *)
            offsetProp(float, dissolveArrow, dissolveArrow, *)
            offsetProp(float, cuttable, cuttable, *)

        } else {
#define multiPathProp(name, func) \
if (!path##name)                                \
    path##name = func;

            multiPathProp(position, MSumTrackPathProps(tracks, Vector3::zero(), time, position))
            multiPathProp(rotation, MMultTrackPathProps(tracks, Quaternion::identity(), time, rotation))
            multiPathProp(scale, MMultTrackPathProps(tracks, Vector3::one(), time, scale))
            multiPathProp(localRotation, MMultTrackPathProps(tracks, Quaternion::identity(), time, localRotation))
            multiPathProp(dissolve, MMultTrackPathProps(tracks, 1.0f, time, dissolve))
            multiPathProp(dissolveArrow, MMultTrackPathProps(tracks, 1.0f, time, dissolveArrow))
            multiPathProp(cuttable, MMultTrackPathProps(tracks, 1.0f, time, cuttable))

            offset.positionOffset = pathposition + MSumTrackProps(tracks, Vector3::zero(), position);
            offset.rotationOffset = pathrotation * MMultTrackProps(tracks, Quaternion::identity(), rotation);
            offset.scaleOffset = pathscale * MMultTrackProps(tracks, Vector3::one(), scale);
            offset.localRotationOffset = pathlocalRotation * MMultTrackProps(tracks, Quaternion::identity(), localRotation);
            offset.dissolve = pathdissolve * MMultTrackProps(tracks, 1.0f, dissolve);
            offset.dissolveArrow = pathdissolveArrow * MMultTrackProps(tracks, 1.0f, dissolveArrow);
            offset.cuttable = pathcuttable * MMultTrackProps(tracks, 1.0f, cuttable);
        }
    } else {
        offset.positionOffset = pathposition;
        offset.rotationOffset = pathrotation;
        offset.scaleOffset = pathscale;
        offset.localRotationOffset = pathlocalRotation;
        offset.dissolve = pathdissolve;
        offset.dissolveArrow = pathdissolveArrow;
        offset.cuttable = pathcuttable;
    }

    if (offset.positionOffset)
        offset.positionOffset = offset.positionOffset.value() * spawnController->beatmapObjectSpawnMovementData->noteLinesDistance;

    return offset;
}
