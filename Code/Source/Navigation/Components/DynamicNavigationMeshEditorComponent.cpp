// Copyright (c) 2021-present Sparky Studios. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <Navigation/Components/DynamicNavigationMeshEditorComponent.h>
#include <Navigation/Utils/Constants.h>

#include <DetourDebugDraw.h>

namespace SparkyStudios::AI::Behave::Navigation
{
    void DynamicNavigationMeshEditorComponent::Reflect(AZ::ReflectContext* rc)
    {
        if (auto* sc = azrtti_cast<AZ::SerializeContext*>(rc))
        {
            DynamicNavigationMeshComponent::Reflect(rc);

            sc->Class<DynamicNavigationMeshEditorComponent, EditorComponentBase>()
                ->Field("Component", &DynamicNavigationMeshEditorComponent::_navMeshComponent)
                ->Field("DebugDraw", &DynamicNavigationMeshEditorComponent::_enableDebug)
                ->Field("DebugDepthTest", &DynamicNavigationMeshEditorComponent::_depthTest)
                ->Field("Settings", &DynamicNavigationMeshEditorComponent::_settings)
                ->Field("Bounds", &DynamicNavigationMeshEditorComponent::_aabb);

            if (AZ::EditContext* ec = sc->GetEditContext())
            {
                ec->Class<DynamicNavigationMeshComponent>(
                      "Dynamic Navigation Mesh", "Setup a navigation mesh to be dynamically built and updated at runtime.")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default, &DynamicNavigationMeshComponent::_settings, "Settings", "Navigation Mesh Settings.")
                    ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::Hide)
                    ->Attribute(AZ::Edit::Attributes::SliceFlags, AZ::Edit::SliceFlags::NotPushable);

                ec->Class<DynamicNavigationMeshEditorComponent>(
                      "Dynamic Navigation Mesh", "Setup a navigation mesh to be dynamically built and updated at runtime.")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Category, "Behave AI")
                    ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC("Game", 0x232b318c))
                    ->Attribute(AZ::Edit::Attributes::FixedComponentListIndex, 0)
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->Attribute(AZ::Edit::Attributes::NameLabelOverride, "Behave AI - Dynamic Navigation Mesh")

                    ->DataElement(
                        AZ::Edit::UIHandlers::Default, &DynamicNavigationMeshEditorComponent::_settings, "Settings",
                        "Settings to use when building the navigation mesh.")

                    ->ClassElement(AZ::Edit::ClassElements::Group, "Debug")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default, &DynamicNavigationMeshEditorComponent::_enableDebug, "Enable",
                        "Draw the navigation mesh.")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default, &DynamicNavigationMeshEditorComponent::_depthTest, "Depth Test",
                        "Enable the depth test while drawing the navigation mesh.")

                    ->DataElement(
                        AZ::Edit::UIHandlers::Default, &DynamicNavigationMeshEditorComponent::_navMeshComponent,
                        "Navigation Mesh Properties", "The Navigation Mesh Component that will be used to build the navigation mesh.")
                    ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::Hide)

                    ->UIElement(AZ::Edit::UIHandlers::Button, "", "Build the navigation mesh with the current settings.")
                    ->Attribute(AZ::Edit::Attributes::ButtonText, "Build")
                    ->Attribute(AZ::Edit::Attributes::ChangeNotify, &DynamicNavigationMeshEditorComponent::OnBuildNavigationMesh);
            }
        }

        if (auto* bc = azrtti_cast<AZ::BehaviorContext*>(rc))
        {
            bc->Enum<static_cast<int>(NavigationMeshPartitionType::Watershed)>("eNMPT_Watershed")
                ->Enum<static_cast<int>(NavigationMeshPartitionType::Monotone)>("eNMPT_Monotone")
                ->Enum<static_cast<int>(NavigationMeshPartitionType::Layers)>("eNMPT_Layers");
        }
    }

    void DynamicNavigationMeshEditorComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC("DynamicNavigationMeshService"));
    }

    void DynamicNavigationMeshEditorComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC("DynamicNavigationMeshService"));
    }

    void DynamicNavigationMeshEditorComponent::GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        required.push_back(AZ_CRC_CE("AxisAlignedBoxShapeService"));
    }

    void DynamicNavigationMeshEditorComponent::GetDependentServices(
        [[maybe_unused]] AZ::ComponentDescriptor::DependencyArrayType& dependent)
    {
    }

    DynamicNavigationMeshEditorComponent::~DynamicNavigationMeshEditorComponent()
    {
    }

    AZ::u32 DynamicNavigationMeshEditorComponent::NavigationMeshSettingsAssetUpdated()
    {
        AZ::Data::AssetBus::Handler::BusDisconnect();

        if (_settings.GetId().IsValid())
        {
            SyncSettings();

            AZ::Data::AssetBus::Handler::BusConnect(_settings.GetId());
            _settings.QueueLoad();
        }
        else
        {
            SetSettings();
        }

        return AZ::Edit::PropertyRefreshLevels::EntireTree;
    }

    const BehaveNavigationMeshSettingsAsset* DynamicNavigationMeshEditorComponent::GetSettings() const
    {
        return _settings.Get();
    }

    const AZ::Aabb& DynamicNavigationMeshEditorComponent::GetBoundingBox() const
    {
        return _aabb;
    }

    void DynamicNavigationMeshEditorComponent::Init()
    {
    }

    void DynamicNavigationMeshEditorComponent::Activate()
    {
        _currentEntityTransform = AZ::Transform::CreateIdentity();
        _navigationMesh = new RecastNavigationMesh(GetEntityId(), true);

        BehaveNavigationMeshNotificationBus::Handler::BusConnect(GetEntityId());
        LmbrCentral::ShapeComponentNotificationsBus::Handler::BusConnect(GetEntityId());
        AZ::TransformNotificationBus::Handler::BusConnect(GetEntityId());
        AzFramework::EntityDebugDisplayEventBus::Handler::BusConnect(GetEntityId());

        _navMeshComponent.Init();

        if (_settings.GetId().IsValid())
        {
            // Re-retrieve the asset in case it was reloaded while we were inactive.
            _settings = AZ::Data::AssetManager::Instance().GetAsset<BehaveNavigationMeshSettingsAsset>(
                _settings.GetId(), AZ::Data::AssetLoadBehavior::Default);
            SetSettings(_settings);

            AZ::Data::AssetBus::Handler::BusConnect(_settings.GetId());
            _settings.QueueLoad();
        }
        else
        {
            SetSettings();
        }
    }

    void DynamicNavigationMeshEditorComponent::Deactivate()
    {
        AZ::Data::AssetBus::Handler::BusDisconnect();

        AzFramework::EntityDebugDisplayEventBus::Handler::BusDisconnect();
        AZ::TransformNotificationBus::Handler::BusDisconnect();
        LmbrCentral::ShapeComponentNotificationsBus::Handler::BusDisconnect();
        BehaveNavigationMeshNotificationBus::Handler::BusDisconnect();

        delete _navigationMesh;
    }

    void DynamicNavigationMeshEditorComponent::BuildGameEntity(AZ::Entity* entity)
    {
        AZ::SerializeContext* context = nullptr;
        AZ::ComponentApplicationBus::BroadcastResult(context, &AZ::ComponentApplicationBus::Events::GetSerializeContext);

        if (!context)
        {
            AZ_Error("BehaveAI", false, "Unable to get a serialize context from component application.");
            return;
        }

        SyncSettings();
        entity->AddComponent(context->CloneObject(&_navMeshComponent));
    }

    void DynamicNavigationMeshEditorComponent::OnTick([[maybe_unused]] float deltaTime, [[maybe_unused]] AZ::ScriptTimePoint time)
    {
    }

    int DynamicNavigationMeshEditorComponent::GetTickOrder()
    {
        return AZ::TICK_DEFAULT;
    }

    void DynamicNavigationMeshEditorComponent::OnNavigationMeshUpdated()
    {
        _waitingOnNavMeshBuild = false;
    }

    void DynamicNavigationMeshEditorComponent::OnShapeChanged(ShapeChangeReasons changeReason)
    {
        if (changeReason == ShapeChangeReasons::ShapeChanged)
            UpdateNavMeshAABB();
    }

    void DynamicNavigationMeshEditorComponent::OnTransformChanged(const AZ::Transform& /*local*/, const AZ::Transform& world)
    {
        _currentEntityTransform = world;
    }

    void DynamicNavigationMeshEditorComponent::DisplayEntityViewport(
        [[maybe_unused]] const AzFramework::ViewportInfo& viewportInfo, AzFramework::DebugDisplayRequests& debugDisplay)
    {
        if (_enableDebug && _navigationMesh->IsNavigationMeshReady() && _navigationMesh->GetNavigationMesh())
        {
            // only uniform scale is supported in physics so the debug visuals reflect this fact
            AZ::Transform worldFromLocalWithUniformScale = _currentEntityTransform;
            worldFromLocalWithUniformScale.SetUniformScale(worldFromLocalWithUniformScale.GetUniformScale());

            debugDisplay.PushMatrix(AZ::Transform::Identity());

            _debugDraw.SetEnableDepthTest(_depthTest);
            _debugDraw.SetDebugDisplayRequestsHandler(&debugDisplay);
            duDebugDrawNavMesh(&_debugDraw, *_navigationMesh->_navMesh, DU_DRAWNAVMESH_OFFMESHCONS);
            // duDebugDrawPolyMeshDetail(&_debugDraw, *_navigationMesh->_detailMesh);
            // duDebugDrawPolyMesh(&_debugDraw, *_navigationMesh->_polyMesh);
            // duDebugDrawCompactHeightfieldSolid(&_debugDraw, *_navigationMesh->_compactHeightField);

            debugDisplay.PopMatrix();
        }
    }

    void DynamicNavigationMeshEditorComponent::OnAssetReady(AZ::Data::Asset<AZ::Data::AssetData> asset)
    {
        SetSettings(asset);
    }

    void DynamicNavigationMeshEditorComponent::OnAssetReloaded(AZ::Data::Asset<AZ::Data::AssetData> asset)
    {
        SetSettings(asset);
    }

    void DynamicNavigationMeshEditorComponent::OnAssetError(AZ::Data::Asset<AZ::Data::AssetData> asset)
    {
        // only notify for asset errors for the asset we care about.
#if defined(AZ_ENABLE_TRACING)
        if ((asset.GetId().IsValid()) && (asset == _settings))
        {
            AZ_Error("BehaveAI", false, "Navigation Mesh Settings asset \"%s\" failed to load.", asset.ToString<AZStd::string>().c_str());
        }
#else // else if AZ_ENABLE_TRACING is not currently defined...
        AZ_UNUSED(asset);
#endif
    }

    void DynamicNavigationMeshEditorComponent::SetSettings(const AZ::Data::Asset<AZ::Data::AssetData>& settings)
    {
        _settings = settings;
        SyncSettings();
    }

    void DynamicNavigationMeshEditorComponent::UpdateNavMeshAABB()
    {
        AZ::Transform transform;
        AZ::Aabb aabb;

        LmbrCentral::ShapeComponentRequestsBus::Event(
            GetEntityId(), &LmbrCentral::ShapeComponentRequestsBus::Events::GetTransformAndLocalBounds, transform, aabb);

        _aabb = AZStd::move(aabb);
        SyncSettings();
    }

    void DynamicNavigationMeshEditorComponent::SyncSettings()
    {
        _navMeshComponent._settings = _settings;
        _navMeshComponent._aabb = _aabb;
    }

    AZ::Crc32 DynamicNavigationMeshEditorComponent::OnBuildNavigationMesh()
    {
        if (_waitingOnNavMeshBuild)
            return AZ::Edit::PropertyRefreshLevels::None;

        //_waitingOnNavMeshBuild = true;
        _navigationMesh->BuildNavigationMesh(this);

        return AZ::Edit::PropertyRefreshLevels::EntireTree;
    }

    AZ::Crc32 DynamicNavigationMeshEditorComponent::GetBuildButtonState()
    {
        return _waitingOnNavMeshBuild ? AZ::Edit::PropertyVisibility::Hide : AZ::Edit::PropertyVisibility::Show;
    }
} // namespace SparkyStudios::AI::Behave::Navigation
