#include "../BetterThanNothing.hpp"

namespace BetterThanNothing
{
	static const std::vector<Vertex> m_vertices = {
		{{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
		{{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
		{{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
		{{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}
	};

	static const std::vector<uint16> m_indices = {
		0, 1, 2, 2, 3, 0
	};

	void VulkanRenderer::Initialize()
	{
		m_pVulkanSwapChain = std::make_unique<VulkanSwapChain>(m_context);
		m_pVulkanSwapChain->CreateImages();
		m_pVulkanSwapChain->CreateImageViews();

		m_pVulkanRenderPass = std::make_unique<VulkanRenderPass>(m_context);

		m_pVulkanSwapChain->CreateFramebuffers();

		m_pVulkanCommandPool = std::make_unique<VulkanCommandPool>(m_context);

		_createCommandBuffers();
		_createSyncObjects();

		m_pBufferFactory = std::make_unique<VulkanBufferFactory>(m_context);


		// Here we create a vertex buffer from a list of vertices and a staging buffer
		{
			const uint32 bufferSize = sizeof(m_vertices[0]) * m_vertices.size();
			const void* bufferData = m_vertices.data();

			m_vertexBuffer = m_pBufferFactory->CreateVertexBuffer(bufferData, bufferSize);
		}

		{
			const uint32 bufferSize = sizeof(m_indices[0]) * m_indices.size();
			const void* bufferData = m_indices.data();

			m_indexBuffer = m_pBufferFactory->CreateIndexBuffer(bufferData, bufferSize);
		}

		LOG_SUCCESS("Renderer: ok");
	}

	void VulkanRenderer::Render()
	{
		m_frameInFlightFences[m_currentFrame]->Wait();

		auto device = m_context->GetVulkanDevice()->LogicalHandle();
		auto swapchain = m_pVulkanSwapChain->Handle();

		uint32 imageIndex;
		VkResult acquireResult = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, m_imageAvailableSemaphores[m_currentFrame]->Handle(), VK_NULL_HANDLE, &imageIndex);

		if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR)
		{
			m_pVulkanSwapChain->RecreateSwapchain();
			return;
		}

		if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR)
			throw std::runtime_error("failed to acquire swap chain image!");

		m_frameInFlightFences[m_currentFrame]->Reset();

		m_commandBuffers[m_currentFrame]->Reset();
		_recordCommandBuffer(imageIndex);

		VkSemaphore waitSemaphores[] = { m_imageAvailableSemaphores[m_currentFrame]->Handle() };

		// We wait for the fragment shader to output the color before we can start writing to the image
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;
		submitInfo.commandBufferCount = 1;

		auto commandBuffer = m_commandBuffers[m_currentFrame]->Handle();
		submitInfo.pCommandBuffers = &commandBuffer;

		VkSemaphore signalSemaphores[] = { m_renderFinishedSemaphores[m_currentFrame]->Handle() };
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;

		auto graphicsQueue = m_context->GetGraphicsQueue()->Handle();
		if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, m_frameInFlightFences[m_currentFrame]->Handle()) != VK_SUCCESS)
			throw std::runtime_error("failed to submit draw command buffer!");

		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;

		VkSwapchainKHR swapChains[] = { swapchain };
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;
		presentInfo.pImageIndices = &imageIndex;

		VkResult presentResult = vkQueuePresentKHR(m_context->GetPresentQueue()->Handle(), &presentInfo);

		if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR || m_context->GetWindow()->IsResized())
		{
			m_context->GetWindow()->SetResized(false);
			m_pVulkanSwapChain->RecreateSwapchain();
		}
		else if (presentResult != VK_SUCCESS)
			throw std::runtime_error("failed to present swap chain image!");

		m_currentFrame = (m_currentFrame + 1) % m_context->GetMaxFrameInFlightCount();
	}

	void VulkanRenderer::_recordCommandBuffer(uint32 imageIndex)
	{
		m_commandBuffers[m_currentFrame]->BeginRecording();

		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = m_pVulkanRenderPass->Handle();
		renderPassInfo.framebuffer = m_pVulkanSwapChain->GetFramebuffer(imageIndex);
		renderPassInfo.renderArea.offset = {0, 0};
		renderPassInfo.renderArea.extent = m_pVulkanSwapChain->GetExtent();

		VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
		renderPassInfo.clearValueCount = 1;
		renderPassInfo.pClearValues = &clearColor;

		m_commandBuffers[m_currentFrame]->CmdBeginRenderPass(&renderPassInfo);

		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = static_cast<float32>(m_pVulkanSwapChain->GetExtent().width);
		viewport.height = static_cast<float32>(m_pVulkanSwapChain->GetExtent().height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		m_commandBuffers[m_currentFrame]->CmdSetViewport(&viewport);

		VkRect2D scissor{};
		scissor.offset = {0, 0};
		scissor.extent = m_pVulkanSwapChain->GetExtent();
		m_commandBuffers[m_currentFrame]->CmdSetScissor(&scissor);

		m_commandBuffers[m_currentFrame]->CmdBindPipeline(m_pPipeline->Handle());

		VkBuffer vertexBuffers[] = { m_vertexBuffer.Handle() };
		VkDeviceSize offsets[] = {0};
		m_commandBuffers[m_currentFrame]->CmdBindVertexBuffers(0, 1, vertexBuffers, offsets);
		m_commandBuffers[m_currentFrame]->CmdBindIndexBuffer(m_indexBuffer.Handle(), 0, VK_INDEX_TYPE_UINT16);

		m_commandBuffers[m_currentFrame]->CmdDrawIndexed(static_cast<uint32>(m_indices.size()), 1, 0, 0, 0);

		m_commandBuffers[m_currentFrame]->CmdEndRenderPass();
		m_commandBuffers[m_currentFrame]->EndRecording();
	}

	void VulkanRenderer::AddPipeline(const std::function<void(VulkanPipelineBuilder&)>& callback)
	{
		VulkanPipelineBuilder builder;
		callback(builder);

		m_pPipeline = std::make_unique<VulkanPipeline>(builder.GetBuildParams(), m_context);
	}

	void VulkanRenderer::_createCommandBuffers()
	{
		auto device = m_context->GetVulkanDevice()->LogicalHandle();

		uint32 count = m_context->GetMaxFrameInFlightCount();

		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = m_pVulkanCommandPool->Handle();
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = 1;

		m_commandBuffers.reserve(count);

		for (uint32 i = 0; i < count; i++)
		{
			VkCommandBuffer commandBuffer;
			if (vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer) != VK_SUCCESS)
				throw std::runtime_error("failed to allocate command buffers!");

			m_commandBuffers.push_back(std::make_unique<VulkanCommandBuffer>(commandBuffer));
		}

		LOG_SUCCESS("Vulkan command buffers: ok (count: " << count << ")");
	}

	void VulkanRenderer::_createSyncObjects()
	{
		uint32 count = m_context->GetMaxFrameInFlightCount();

		m_imageAvailableSemaphores.reserve(count);
		m_renderFinishedSemaphores.reserve(count);
		m_frameInFlightFences.reserve(count);

		for (uint32 i = 0; i < count; i++)
		{
			m_imageAvailableSemaphores.push_back(std::make_unique<VulkanSemaphore>(m_context));
			m_renderFinishedSemaphores.push_back(std::make_unique<VulkanSemaphore>(m_context));
			m_frameInFlightFences.push_back(std::make_unique<VulkanFence>(m_context));
		}

		LOG_SUCCESS("Vulkan sync objects: ok (count: " << count << ")");
	}

	std::unique_ptr<VulkanSwapChain>& VulkanRenderer::GetVulkanSwapChain()
	{
		if (!m_pVulkanSwapChain)
			throw ApplicationContextError("Vulkan swap chain is not set");
		return m_pVulkanSwapChain;
	}

	std::unique_ptr<VulkanRenderPass>& VulkanRenderer::GetVulkanRenderPass()
	{
		if (!m_pVulkanRenderPass)
			throw ApplicationContextError("Vulkan render pass is not set");
		return m_pVulkanRenderPass;
	}

	std::unique_ptr<VulkanCommandPool>& VulkanRenderer::GetVulkanCommandPool()
	{
		if (!m_pVulkanCommandPool)
			throw ApplicationContextError("Vulkan command pool is not set");
		return m_pVulkanCommandPool;
	}
}