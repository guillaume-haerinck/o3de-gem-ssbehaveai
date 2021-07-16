#include <StdAfx.h>

#include <SparkyStudios/AI/BehaviorTree/Core/SSBehaviorTreeFactory.h>

namespace SparkyStudios::AI::BehaviorTree::Core
{
    SSBehaviorTreeFactory::SSBehaviorTreeFactory(SSBehaviorTreeRegistry* registry)
    {
        if (registry)
            m_registry = AZStd::shared_ptr<SSBehaviorTreeRegistry>(registry);
        else
            m_registry = AZStd::shared_ptr<SSBehaviorTreeRegistry>(aznew SSBehaviorTreeRegistry());
    }

    SSBehaviorTreeFactory::~SSBehaviorTreeFactory()
    {
        m_registry.reset();
    }

    const AZStd::shared_ptr<SSBehaviorTreeRegistry>& SSBehaviorTreeFactory::GetRegistry() const
    {
        return m_registry;
    }

    AZStd::unique_ptr<SSBehaviorTreeBlackboardProperty> SSBehaviorTreeFactory::CreateProperty(
        const AZStd::string& type, const char* name) const
    {
        auto it = m_registry->m_registeredTypeBuilders.find(type);
        if (it != m_registry->m_registeredTypeBuilders.end())
            return it->second(name);

        return nullptr;
    }

    AZStd::unique_ptr<SSBehaviorTreeNode> SSBehaviorTreeFactory::CreateNode(
        const AZStd::string& name, const AZStd::string& instanceName, const SSBehaviorTreeNodeConfiguration& config) const
    {
        auto it = m_registry->m_registeredNodeBuilders.find(name);
        if (it != m_registry->m_registeredNodeBuilders.end())
        {
            auto node = it->second(instanceName.c_str(), config);
            node->setRegistrationID(name.c_str());
        }

        throw std::runtime_error("Unable to create the node. Did you forget to register?");
    }

    BT::Tree SSBehaviorTreeFactory::CreateTreeFromText(AZStd::string& text, const Blackboard::SSBehaviorTreeBlackboard& blackboard)
    {
        return m_registry->m_factory->createTreeFromText(text.c_str(), blackboard.m_blackboard);
    }
} // namespace SparkyStudios::AI::BehaviorTree::Core
