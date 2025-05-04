#pragma once

#include <VECS.h>
#include <VECSRegistry.h>

namespace vecs {

    //----------------------------------------------------------------------------------------------
	//Manager

    /// @brief A manager for handling registries and parallel access
    class Manager {
    public:
        Manager() {}

        ~Manager() = default;  // Destructor


		/// @brief Get a view of entities with specific components.
		/// @tparam ...Ts The types of the components.
		/// @return A view of the entity components
		template<typename... Ts>
			requires (vtll::unique<vtll::tl<Ts...>>::value)
		[[nodiscard]] auto GetView(std::vector<size_t>&& yes={}, std::vector<size_t>&& no={}) -> Registry::View<Ts...> {
			return m_system.GetView<Ts...>(std::forward<std::vector<size_t>>(yes), std::forward<std::vector<size_t>>(no));
		}


        // TODO: check if component exists (relevant for paralell)
        
        /// @brief Get a single component value of an entity.
		/// @tparam T The type of the component.
		/// @param handle The handle of the entity.
		/// @return The component value or reference to it.
        template<typename T>
        auto GetComponent(Handle handle) -> Registry::to_ref_t<T> {
            return m_system.Get<T>(handle);
        }

        /// @brief Get multiple component values of an entity.
		/// @tparam Ts The types of the components.
		/// @param handle The handle of the entity.
		/// @return A tuple of the component values.
        template<typename... Ts>
            requires (sizeof...(Ts)>1 && vtll::unique<vtll::tl<Ts...>>::value && !vtll::has_type< vtll::tl<Ts...>, Handle&>::value)
        auto GetComponent(Handle handle) -> std::tuple<Registry::to_ref_t<Ts>...> {
            return m_system.Get<Ts...>(handle);
        }




        /// @brief Create an entity with components.
		/// @tparam ...Ts The types of the components.
		/// @param ...component The new values.
		/// @return Handle of new entity.
		template<typename... Ts>
			requires ((sizeof...(Ts) > 0) && (vtll::unique<vtll::tl<Ts...>>::value) && !vtll::has_type< vtll::tl<Ts...>, Handle>::value)
		[[nodiscard]] auto CreateEntity( Ts&&... component ) -> Handle {
            return m_system.Insert(std::forward<Ts>(component)...);
        }


        /// @brief Put new component values as a tuple to an entity.
		/// @tparam Ts The types of the components.
		/// @param handle The handle of the entity.
		/// @param v The new values in a tuple
		template<typename... Ts>
        requires (vtll::unique<vtll::tl<Ts...>>::value && !vtll::has_type< vtll::tl<std::decay_t<Ts>...>, Handle>::value)
        void PutComponent(Handle handle, std::tuple<Ts...>& v) {
            m_system.Put(handle, v);
        }

        /// @brief Put new component values to an entity.
        /// @tparam Ts The types of the components.
        /// @param handle The handle of the entity.
        /// @param vs The new values.
        template<typename... Ts>
            requires ((vtll::unique<vtll::tl<Ts...>>::value) && !vtll::has_type< vtll::tl<std::decay_t<Ts>...>, Handle>::value)
        void PutComponent(Handle handle, Ts&&... vs) {
            m_system.Put(handle, std::forward<Ts>(vs)...);
        }


        /// @brief Add tags to an entity.
		/// @tparam ...Ts The types of the tags.
		/// @param handle The handle of the entity.
		/// @param ...tags The tags to add.
		template<typename... Ts>
        requires (std::is_integral_v<std::decay_t<Ts>> && ...)
        void AddTags(Handle handle, Ts... tags) {
            m_system.AddTags(handle, std::forward<Ts>(tags)...);
        }


        /// @brief Erase tags from an entity.
        /// @tparam ...Ts The types of the tags.
        /// @param handle The handle of the entity.
        /// @param ...tags The tags to erase.
        template<typename... Ts>
            requires (std::is_integral_v<std::decay_t<Ts>> && ...)
        void EraseTags(Handle handle, Ts... tags) {
            m_system.EraseTags(handle,std::forward<Ts>(tags)...);
        }


        /// @brief Erase components from an entity.
		/// @tparam ...Ts The types of the components.
		/// @param handle The handle of the entity.		
		template<typename... Ts>
            requires (vtll::unique<vtll::tl<Ts...>>::value && !vtll::has_type< vtll::tl<Ts...>, Handle>::value)
        void EraseComponents(Handle handle) {
            m_system.Erase<Ts...>(handle);
        }

        void EraseEntity(Handle handle) {
            m_system.Erase(handle);
        }


        /// @brief Clear the registry by removing all entities.
		void ClearRegistry() {
			m_system.Clear();
		}


    private:
        vecs::Registry m_system;
        
    };

}