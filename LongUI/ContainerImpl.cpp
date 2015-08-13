﻿#include "LongUI.h"

// -------------------------- UIContainer -------------------------
// UIContainer 构造函数
LongUI::UIContainer::UIContainer(pugi::xml_node node) noexcept : Super(node), marginal_control(){
    ::memset(force_cast(marginal_control), 0, sizeof(marginal_control));
    assert(node && "bad argument.");
    // 保留原始外间距
    m_orgMargin = this->margin_rect;
    auto flag = this->flags | Flag_UIContainer;
    // 检查边缘控件
    {
        // 属性ID
        const char* const attname[] = {
            LongUI::XMLAttribute::LeftMarginalControl,
            LongUI::XMLAttribute::TopMarginalControl,
            LongUI::XMLAttribute::RightMarginalControl,
            LongUI::XMLAttribute::bottomMarginalControl,
        };
        bool exist_marginal_control = false;
        // ONLY MY LOOPGUN
        for (auto i = 0u; i < UIMarginalable::MARGINAL_CONTROL_SIZE; ++i) {
            const char* str = nullptr;
            // 获取指定属性值
            if ((str = node.attribute(attname[i]).value())) {
                char buffer[LongUIStringLength];
                assert(::strlen(str) < LongUIStringLength && "buffer too small");
                // 获取逗号位置
                auto strtempid = ::strchr(str, ',');
                if (strtempid) {
                    auto length = strtempid - str;
                    ::memcpy(buffer, str, length);
                    buffer[length] = char(0);
                }
                else {
                    ::strcpy(buffer, str);
                }
                // 获取类ID
                auto create_control_func = UIManager.GetCreateFunc(buffer);
                assert(create_control_func && "none");
                if (create_control_func) {
                    // 检查模板ID
                    auto tid = strtempid ? LongUI::AtoI(strtempid + 1) : 0;
                    // 创建控件
                    auto control = UIManager.CreateControl(size_t(tid), create_control_func);
                    // XXX: 检查
                    force_cast(this->marginal_control[i]) = longui_cast<UIMarginalable*>(control);
                }
                // 优化flag
                if (this->marginal_control[i]) {
                    exist_marginal_control = true;
                }
            }
        }
        // 存在
        if (exist_marginal_control) {
            flag |= Flag_Container_ExistMarginalControl;
        }
    }
    // 渲染依赖属性
    if ((this->flags & Flag_RenderParent) || 
        node.attribute(LongUI::XMLAttribute::IsRenderChildrenD).as_bool(false)) {
        flag |= LongUI::Flag_Container_AlwaysRenderChildrenDirectly;
    }
    force_cast(this->flags) = flag;
}

// UIContainer 析构函数
LongUI::UIContainer::~UIContainer() noexcept {
    // 关闭边缘控件
    // 只有一次 Flag_Container_ExistMarginalControl 可用可不用
    for (auto ctrl : this->marginal_control) {
        if (ctrl) {
            ctrl->Cleanup();
        }
    }
    // 关闭子控件
    {
        auto ctrl = m_pHead;
        while (ctrl) { 
            auto next_ctrl = ctrl->next; 
            ctrl->Cleanup(); 
            ctrl = next_ctrl; 
        }
    }
}

// 插入后处理
void LongUI::UIContainer::AfterInsert(UIControl* child) noexcept {
    assert(child && "bad argument");
    // 大小判断
    if (this->size() >= 10'000) {
        UIManager << DL_Warning << "the count of children must be"
            " less than 10k because of the precision of float" << LongUI::endl;
        assert(!"because of the precision of float, the count of children must be less than 10k");
    }
    // 检查flag
    if (this->flags & Flag_Container_AlwaysRenderChildrenDirectly) {
        force_cast(child->flags) |= Flag_RenderParent;
    }
    // 设置父类
    force_cast(child->parent) = this;
    // 设置窗口节点
    child->m_pWindow = m_pWindow;
    // 重建资源
    child->Recreate(m_pRenderTarget);
    // 修改
    child->SetControlSizeChanged();
    // 修改
    this->SetControlSizeChanged();
}

/// <summary>
/// Find the control via mouse point
/// </summary>
/// <param name="pt">The wolrd mouse point.</param>
/// <returns>the control pointer, maybe nullptr</returns>
auto LongUI::UIContainer::FindControl(const D2D1_POINT_2F pt) noexcept->UIControl* {
    // 查找边缘控件
    if (this->flags & Flag_Container_ExistMarginalControl) {
        for (auto ctrl : this->marginal_control) {
            if (ctrl && IsPointInRect(ctrl->visible_rect, pt)) {
                return ctrl;
            }
        }
    }
    this->AssertMarginalControl();
    UIControl* control_out = nullptr;
    // XXX: 优化
    assert(this->size() < 100 && "too huge, wait for optimization please");
    for (auto ctrl : (*this)) {
        /*if (m_strControlName == L"MainWindow") {
            int a = 9;
        }*/
        // 区域内判断
        if (IsPointInRect(ctrl->visible_rect, pt)) {
            if (ctrl->flags & Flag_UIContainer) {
                control_out = static_cast<UIContainer*>(ctrl)->FindControl(pt);
            }
            else {
                control_out = ctrl;
            }
            break;
        }
    }
    return control_out;
}


// do event 事件处理
bool LongUI::UIContainer::DoEvent(const LongUI::EventArgument& arg) noexcept {
    // TODO: 参数EventArgument改为const
    bool done = false;
    // 转换坐标
    // 处理窗口事件
    if (arg.sender) {
        switch (arg.event)
        {
        case LongUI::Event::Event_TreeBulidingFinished:
            // 初始化边缘控件 
            for (auto i = 0; i < lengthof(this->marginal_control); ++i) {
                auto ctrl = this->marginal_control[i];
                if (ctrl) {
                    this->AfterInsert(ctrl);
                    // 初始化
                    ctrl->InitMarginalControl(static_cast<UIMarginalable::MarginalControl>(i));
                    // 完成控件树
                    ctrl->DoEvent(arg);
                }
            }
            // 初次完成空间树建立
            for (auto ctrl : (*this)) {
                ctrl->DoEvent(arg);
            }
            done = true;
            break;
        }
    }
    // 扳回来
    return done;
}

// UIContainer 渲染函数
void LongUI::UIContainer::Render(RenderType type) const noexcept {
    //  正确渲染控件
    auto do_render = [](ID2D1RenderTarget* const target, const UIControl* const ctrl) {
        // 可渲染?
        if (ctrl->visible && ctrl->visible_rect.right > ctrl->visible_rect.left 
            && ctrl->visible_rect.bottom > ctrl->visible_rect.top) {
            // 修改世界转换矩阵
            target->SetTransform(&ctrl->world);
            // 检查剪切规则
            if (ctrl->flags & Flag_ClipStrictly) {
                D2D1_RECT_F clip_rect; ctrl->GetRectAll(clip_rect);
                target->PushAxisAlignedClip(&clip_rect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
            }
            ctrl->Render(LongUI::RenderType::Type_Render);
            // 检查剪切规则
            if (ctrl->flags & Flag_ClipStrictly) {
                target->PopAxisAlignedClip();
            }
        }
    };
    // 查看
    switch (type)
    {
    case LongUI::RenderType::Type_RenderBackground:
        __fallthrough;
    case LongUI::RenderType::Type_Render:
        // 父类背景
        Super::Render(LongUI::RenderType::Type_RenderBackground);
        // 背景中断
        if (type == LongUI::RenderType::Type_RenderBackground) {
            break;
        }
        // 普通子控件仅仅允许渲染在内容区域上
        {
            D2D1_RECT_F clip_rect; this->GetViewRect(clip_rect);
            m_pRenderTarget->PushAxisAlignedClip(&clip_rect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        }
        // 渲染所有子部件
        for(const auto* ctrl : (*this)) {
            do_render(m_pRenderTarget, ctrl);
        }
        // 弹出
        m_pRenderTarget->PopAxisAlignedClip();
        // 渲染边缘控件
        if (this->flags & Flag_Container_ExistMarginalControl) {
            for (auto ctrl : this->marginal_control) {
                if (ctrl) {
                    do_render(m_pRenderTarget, ctrl);
                }
            }
        }
        this->AssertMarginalControl();
        __fallthrough;
    case LongUI::RenderType::Type_RenderForeground:
        // 父类前景
        //Super::Render(LongUI::RenderType::Type_RenderForeground);
        break;
    case LongUI::RenderType::Type_RenderOffScreen:
        break;
    }
}

// 刷新边缘控件
void LongUI::UIContainer::update_marginal_controls() noexcept {
    // 获取宽度
    auto get_marginal_width = [](UIMarginalable* ctrl) noexcept {
        return ctrl ? ctrl->marginal_width : 0.f;
    };
    // 利用规则获取宽度
    auto get_marginal_width_with_rule = [](UIMarginalable* a, UIMarginalable* b) noexcept {
        return a->rule == UIMarginalable::Rule_Greedy ? 0.f : (b ? b->marginal_width : 0.f);
    };
    // 计算宽度
    auto caculate_container_width = [this, get_marginal_width]() noexcept {
        // 基本宽度
        return this->view_size.width 
            + m_orgMargin.left
            + m_orgMargin.right
            + get_marginal_width(this->marginal_control[UIMarginalable::Control_Left])
            + get_marginal_width(this->marginal_control[UIMarginalable::Control_Right])
            + m_fBorderWidth * 2.f;
    };    
    // 计算高度
    auto caculate_container_height = [this, get_marginal_width]() noexcept {
        // 基本宽度
        return this->view_size.height 
            + m_orgMargin.top
            + m_orgMargin.bottom
            + get_marginal_width(this->marginal_control[UIMarginalable::Control_Top])
            + get_marginal_width(this->marginal_control[UIMarginalable::Control_Bottom])
            + m_fBorderWidth * 2.f;
    };
    // 保留信息
    const float this_container_width = caculate_container_width();
    const float this_container_height = caculate_container_height();
    // 循环
    while (true) {
        // XXX: 优化
        for (auto i = 0u; i < lengthof(this->marginal_control); ++i) {
            // 获取控件
            auto ctrl = this->marginal_control[i]; if (!ctrl) continue;
            //float view[] = { 0.f, 0.f, 0.f, 0.f };
            // TODO: 计算cross 大小
            switch (i)
            {
            case 0: // Left
            {
                const auto tmptop = m_orgMargin.top +
                    get_marginal_width_with_rule(ctrl, this->marginal_control[UIMarginalable::Control_Top]);
                // 坐标
                ctrl->SetLeft(m_orgMargin.left);
                ctrl->SetTop(tmptop);
                // 大小
                ctrl->SetWidth(ctrl->marginal_width);
                ctrl->SetHeight(
                    this_container_height - tmptop - m_orgMargin.bottom -
                    get_marginal_width_with_rule(ctrl, this->marginal_control[UIMarginalable::Control_Bottom])
                    );
            }
                break;
            case 1: // Top
            {
                const float tmpleft = m_orgMargin.left +
                    get_marginal_width_with_rule(ctrl, this->marginal_control[UIMarginalable::Control_Left]);
                // 坐标
                ctrl->SetLeft(tmpleft);
                ctrl->SetTop(m_orgMargin.top);
                // 大小
                ctrl->SetWidth(
                    this_container_width - tmpleft - m_orgMargin.right -
                    get_marginal_width_with_rule(ctrl, this->marginal_control[UIMarginalable::Control_Right])
                    );
                ctrl->SetHeight(ctrl->marginal_width);
            }
                break;
            case 2: // Right
            {
                const auto tmptop = m_orgMargin.top +
                    get_marginal_width_with_rule(ctrl, this->marginal_control[UIMarginalable::Control_Top]);
                // 坐标
                ctrl->SetLeft(this_container_width - m_orgMargin.right - ctrl->marginal_width);
                ctrl->SetTop(tmptop);
                // 大小
                ctrl->SetWidth(ctrl->marginal_width);
                ctrl->SetHeight(
                    this_container_height - tmptop - m_orgMargin.bottom -
                    get_marginal_width_with_rule(ctrl, this->marginal_control[UIMarginalable::Control_Bottom])
                    );
            }
                break;
            case 3: // Bottom
            {
                const float tmpleft = m_orgMargin.left +
                    get_marginal_width_with_rule(ctrl, this->marginal_control[UIMarginalable::Control_Left]);
                // 坐标
                ctrl->SetLeft(tmpleft);
                ctrl->SetTop(this_container_height - m_orgMargin.bottom - ctrl->marginal_width);
                // 大小
                ctrl->SetWidth(
                    this_container_width - tmpleft - m_orgMargin.right -
                    get_marginal_width_with_rule(ctrl, this->marginal_control[UIMarginalable::Control_Right])
                    );
                ctrl->SetHeight(ctrl->marginal_width);
            }
                break;
            }
            // 更新边界
            ctrl->UpdateMarginalWidth();
        }
        // 退出检查
        {
            const float latest_width = caculate_container_width();
            const float latest_height = caculate_container_height();
            // 一样就退出
            if (latest_width == this_container_width && latest_height == this_container_height) {
                break;
            }
            // 修改外边距
            force_cast(this->margin_rect.left) = m_orgMargin.left
                + get_marginal_width(this->marginal_control[UIMarginalable::Control_Left]);
            force_cast(this->margin_rect.top) = m_orgMargin.top
                + get_marginal_width(this->marginal_control[UIMarginalable::Control_Top]);
            force_cast(this->margin_rect.right) = m_orgMargin.right
                + get_marginal_width(this->marginal_control[UIMarginalable::Control_Right]);
            force_cast(this->margin_rect.bottom) = m_orgMargin.bottom
                + get_marginal_width(this->marginal_control[UIMarginalable::Control_Bottom]);
            // 修改大小
            this->SetWidth(this_container_width);
            this->SetHeight(this_container_height);
        }
    }
}


// UI容器: 刷新
void LongUI::UIContainer::Update() noexcept  {
    // 修改边界
    if (this->IsControlSizeChanged()) {
        // 刷新边缘控件
        if (this->flags & Flag_Container_ExistMarginalControl) {
            this->update_marginal_controls();
        }
    }
    // 修改可视化区域
    if (this->IsNeedRefreshWorld()) {
        // 更新边缘控件
        if (this->flags & Flag_Container_ExistMarginalControl) {
            for (auto ctrl : this->marginal_control) {
                // 刷新
                if (ctrl) {
                    ctrl->Update();
                    // 更新世界矩阵
                    ctrl->SetControlWorldChanged();
                    ctrl->RefreshWorldMarginal();
                    // 坐标转换
                    D2D1_RECT_F clip_rect; ctrl->GetRectAll(clip_rect);
                    auto lt = LongUI::TransformPoint(ctrl->world, reinterpret_cast<D2D1_POINT_2F&>(clip_rect.left));
                    auto rb = LongUI::TransformPoint(ctrl->world, reinterpret_cast<D2D1_POINT_2F&>(clip_rect.right));
                    // 修改可视区域
                    ctrl->visible_rect.left = std::max(lt.x, this->visible_rect.left);
                    ctrl->visible_rect.top = std::max(lt.y, this->visible_rect.top);
                    ctrl->visible_rect.right = std::min(rb.x, this->visible_rect.right);
                    ctrl->visible_rect.bottom = std::min(rb.y, this->visible_rect.bottom);
                }
            }
        }
        // 本容器内容限制
        D2D1_RECT_F limit_of_this = {
            this->visible_rect.left + this->margin_rect.left * this->world._11,
            this->visible_rect.top + this->margin_rect.top * this->world._22,
            this->visible_rect.right - this->margin_rect.right * this->world._11,
            this->visible_rect.bottom - this->margin_rect.bottom * this->world._22,
        };
        // 更新一般控件
        for (auto ctrl : (*this)) {
            // 更新世界矩阵
            ctrl->SetControlWorldChanged();
            ctrl->RefreshWorld();
            // 坐标转换
            D2D1_RECT_F clip_rect; ctrl->GetRectAll(clip_rect);
            auto lt = LongUI::TransformPoint(ctrl->world, reinterpret_cast<D2D1_POINT_2F&>(clip_rect.left));
            auto rb = LongUI::TransformPoint(ctrl->world, reinterpret_cast<D2D1_POINT_2F&>(clip_rect.right));
            // 限制
            /*if (ctrl->IsCanbeCastedTo(LongUI::GetIID<UIVerticalLayout>())) {
                int bk = 9;
            }*/
            ctrl->visible_rect.left = std::max(lt.x, limit_of_this.left);
            ctrl->visible_rect.top = std::max(lt.y, limit_of_this.top);
            ctrl->visible_rect.right = std::min(rb.x, limit_of_this.right);
            ctrl->visible_rect.bottom = std::min(rb.y, limit_of_this.bottom);
            // 调试信息
            //UIManager << DL_Hint << ctrl << ctrl->visible_rect << endl;
        }
        // 调试信息
        if (this->IsTopLevel()) {
            //UIManager << DL_Log << "Handle: ControlSizeChanged" << LongUI::endl;
        }
        // 已处理该消息
        this->ControlSizeChangeHandled();
    }
    // 刷新边缘控件
    if (this->flags & Flag_Container_ExistMarginalControl) {
#if 1
        for (auto ctrl : this->marginal_control) {
            if(ctrl) ctrl->Update();
        }
#else
        {
            UIMarginalable* ctrl = nullptr;
            if ((ctrl = this->marginal_control[UIMarginalable::Control_Left])) ctrl->Update();
            if ((ctrl = this->marginal_control[UIMarginalable::Control_Top])) ctrl->Update();
            if ((ctrl = this->marginal_control[UIMarginalable::Control_Right])) ctrl->Update();
            if ((ctrl = this->marginal_control[UIMarginalable::Control_Bottom])) ctrl->Update();
        }
#endif
    }
    this->AssertMarginalControl();
    // 刷新一般子控件
    for (auto ctrl : (*this)) ctrl->Update();
    // 刷新父类
    return Super::Update();
}


// UIContainer 重建
auto LongUI::UIContainer::Recreate(LongUIRenderTarget* newRT) noexcept ->HRESULT {
    auto hr = S_OK;
    // 重建边缘控件
    if (this->flags & Flag_Container_ExistMarginalControl) {
        for (auto ctrl : this->marginal_control) {
            if (ctrl && SUCCEEDED(hr)) {
                hr = ctrl->Recreate(newRT);
            }
        }
    }
    this->AssertMarginalControl();
    // 重建父类
    if (SUCCEEDED(hr)) {
        hr = Super::Recreate(newRT);
    }
    return hr;
}

// 设置水平偏移值
LongUINoinline void LongUI::UIContainer::SetOffsetXByChild(float value) noexcept {
    assert(value > -1'000'000.f && value < 1'000'000.f &&
        "maybe so many children in this container that over single float's precision");
    register float target = value * this->GetZoomX();
    if (target != m_2fOffset.x) {
        m_2fOffset.x = target;
        this->SetControlWorldChanged();
    }
}

// 设置垂直偏移值
LongUINoinline void LongUI::UIContainer::SetOffsetYByChild(float value) noexcept {
    assert(value > (-1'000'000.f) && value < 1'000'000.f &&
        "maybe so many children in this container that over single float's precision");
    register float target = value * this->GetZoomY();
    if (target != m_2fOffset.y) {
        m_2fOffset.y = target;
        this->SetControlWorldChanged();
        //UIManager << DL_Hint << "SetControlWorldChanged" << endl;
    }
}

// 获取指定控件
auto LongUI::UIContainer::at(uint32_t i) const noexcept -> UIControl * {
    // 性能警告
    UIManager << DL_Warning 
        << L"Performance Warning! random accessig is not fine for list" 
        << LongUI::endl;
    // 检查范围
    if (i >= this->size()) {
        UIManager << DL_Error << L"out of range" << LongUI::endl;
        return nullptr;
    }
    // 只有一个?
    if (this->size() == 1) return m_pHead;
    // 前半部分?
    UIControl * control;
    if (i < this->size() / 2) {
        control = m_pHead;
        while (i) {
            assert(control && "null pointer");
            control = control->next;
            --i;
        }
    }
    // 后半部分?
    else {
        control = m_pTail;
        i = static_cast<uint32_t>(this->size()) - i - 1;
        while (i) {
            assert(control && "null pointer");
            control = control->prev;
            --i;
        }
    }
    return control;
}

// 插入控件
void LongUI::UIContainer::insert(Iterator itr, UIControl* ctrl) noexcept {
    const auto end_itr = this->end();
    assert(ctrl && "bad arguments");
    if (ctrl->prev) {
        UIManager << DL_Warning
            << L"the 'prev' attr of the control: ["
            << ctrl->GetNameStr()
            << "] that to insert is not null"
            << LongUI::endl;
    }
    if (ctrl->next) {
        UIManager << DL_Warning
            << L"the 'next' attr of the control: ["
            << ctrl->GetNameStr()
            << "] that to insert is not null"
            << LongUI::endl;
    }
    // 插入尾部?
    if (itr == end_itr) {
        // 链接
        force_cast(ctrl->prev) = m_pTail;
        // 无尾?
        if(m_pTail) force_cast(m_pTail->next) = ctrl;
        // 无头?
        if (!m_pHead) m_pHead = ctrl;
        // 设置尾
        m_pTail = ctrl;
    }
    else {
        force_cast(ctrl->next) = itr.Ptr();
        force_cast(ctrl->prev) = itr->prev;
        // 前面->next = ctrl
        // itr->prev = ctrl
        if (itr->prev) {
            force_cast(itr->prev) = ctrl;
        }
        force_cast(itr->prev) = ctrl;
    }
    ++m_cChildrenCount;
    // 添加之后的处理
    this->AfterInsert(ctrl);
}


// 移除控件
bool LongUI::UIContainer::remove(Iterator itr) noexcept {
    // 检查是否属于本容器
#ifdef _DEBUG
    bool ok = false;
    for (auto i : (*this)) {
        if (itr == i) {
            ok = true;
            break;
        }
    }
    if (!ok) {
        UIManager << DL_Error << "control:[" << itr->GetNameStr()
            << "] not in this container: " << this->GetNameStr() << LongUI::endl;
        return false;
    }
#endif
    // 连接前后节点
    register auto prev_tmp = itr->prev;
    register auto next_tmp = itr->next;
    // 检查, 头
    (prev_tmp ? force_cast(prev_tmp->next) : m_pHead) = next_tmp;
    // 检查, 尾
    (next_tmp ? force_cast(next_tmp->prev) : m_pTail) = prev_tmp;
    // 减少
    force_cast(itr->prev) = force_cast(itr->next) = nullptr;
    --m_cChildrenCount;
    // 修改
    this->SetControlSizeChanged();
    return true;
}


// -------------------------- UIVerticalLayout -------------------------
// UIVerticalLayout 创建
auto LongUI::UIVerticalLayout::CreateControl(CreateEventType type, pugi::xml_node node) noexcept ->UIControl* {
    UIControl* pControl = nullptr;
    switch (type)
    {
    case Type_CreateControl:
        if (!node) {
            UIManager << DL_Warning << L"node null" << LongUI::endl;
        }
        // 申请空间
        pControl = LongUI::UIControl::AllocRealControl<LongUI::UIVerticalLayout>(
            node,
            [=](void* p) noexcept { new(p) UIVerticalLayout(node); }
        );
        if (!pControl) {
            UIManager << DL_Error << L"alloc null" << LongUI::endl;
        }
        break;
    case LongUI::Type_Initialize:
        break;
    case LongUI::Type_Recreate:
        break;
    case LongUI::Type_Uninitialize:
        break;
    }
    return pControl;
}

// 更新子控件布局
void LongUI::UIVerticalLayout::Update() noexcept {
    // 基本算法:
    // 1. 去除浮动控件影响
    // 2. 一次遍历, 检查指定高度的控件, 计算基本高度/宽度
    // 3. 计算实际高度/宽度
    if (this->IsControlSizeChanged()) {
        // 初始化
        float base_width = 0.f, base_height = 0.f;
        float counter = 0.0f;
        /*if (m_strControlName == L"MainWindow") {
            int a = 0;
        }*/
        // 第一次
        for (auto ctrl : (*this)) {
            // 非浮点控件
            if (!(ctrl->flags & Flag_Floating)) {
                // 宽度固定?
                if (ctrl->flags & Flag_WidthFixed) {
                    base_width = std::max(base_width, ctrl->GetTakingUpWidth());
                }
                // 高度固定?
                if (ctrl->flags & Flag_HeightFixed) {
                    base_height += ctrl->GetTakingUpHeight();
                }
                // 未指定高度?
                else {
                    counter += 1.f;
                }
            }
        }
        // 校正
        base_width /= m_2fZoom.width;
        base_height /= m_2fZoom.height;
        // 计算
        base_width = std::max(base_width, this->view_size.width);
        // 高度步进
        float height_step = counter > 0.f ? (this->view_size.height - base_height) / counter : 0.f;
        // 小于阈值
        if (height_step < float(LongUIAutoControlMinSize)) {
            height_step = float(LongUIAutoControlMinSize);
        }
        float position_y = 0.f;
        // 第二次
        for (auto ctrl : (*this)) {
            // 浮点控
            if (ctrl->flags & Flag_Floating) continue;
            // 设置控件宽度
            if (!(ctrl->flags & Flag_WidthFixed)) {
                ctrl->SetWidth(base_width);
            }
            // 设置控件高度
            if (!(ctrl->flags & Flag_HeightFixed)) {
                ctrl->SetHeight(height_step);
            }
            // 容器?
            // 不管如何, 修改!
            ctrl->SetControlSizeChanged();
            if (ctrl->GetName() == L"btn_x") {
                int bk = 9;
            }
            ctrl->SetLeft(0.f);
            ctrl->SetTop(position_y);
            position_y += ctrl->GetTakingUpHeight();
        }
        // 修改
        m_2fContentSize.width = base_width;
        m_2fContentSize.height = position_y;
        /*if (m_strControlName == L"MainWindow") {
            int a = 0;
        }*/
        // 已经处理
        this->ControlSizeChangeHandled();
    }
    // 父类刷新
    return Super::Update();
}


// UIVerticalLayout 重建
auto LongUI::UIVerticalLayout::Recreate(LongUIRenderTarget* newRT) noexcept ->HRESULT {
    HRESULT hr = S_OK;
    for (auto ctrl : (*this)) {
        hr = ctrl->Recreate(newRT);
        AssertHR(hr);
    }
    return Super::Recreate(newRT);
}

// UIVerticalLayout 关闭控件
void LongUI::UIVerticalLayout::Cleanup() noexcept {
    delete this;
}

// -------------------------- UIHorizontalLayout -------------------------
// UIHorizontalLayout 创建
auto LongUI::UIHorizontalLayout::CreateControl(CreateEventType type, pugi::xml_node node) noexcept ->UIControl* {
    UIControl* pControl = nullptr;
    switch (type)
    {
    case Type_CreateControl:
        if (!node) {
            UIManager << DL_Warning << L"node null" << LongUI::endl;
        }
        // 申请空间
        pControl = LongUI::UIControl::AllocRealControl<LongUI::UIHorizontalLayout>(
            node,
            [=](void* p) noexcept { new(p) UIHorizontalLayout(node); }
        );
        if (!pControl) {
            UIManager << DL_Error << L"alloc null" << LongUI::endl;
        }
        break;
    case LongUI::Type_Initialize:
        break;
    case LongUI::Type_Recreate:
        break;
    case LongUI::Type_Uninitialize:
        break;
    }
    return pControl;
}


// 更新子控件布局
void LongUI::UIHorizontalLayout::Update() noexcept {
    // 基本算法:
    // 1. 去除浮动控件影响
    // 2. 一次遍历, 检查指定高度的控件, 计算基本高度/宽度
    // 3. 计算实际高度/宽度
    if (this->IsControlSizeChanged()) {
        // 初始化
        float base_width = 0.f, base_height = 0.f;
        float counter = 0.0f;
        // 第一次
        for (auto ctrl : (*this)) {
            // 非浮点控件
            if (!(ctrl->flags & Flag_Floating)) {
                // 高度固定?
                if (ctrl->flags & Flag_HeightFixed) {
                    base_height = std::max(base_height, ctrl->GetTakingUpHeight());
                }
                // 宽度固定?
                if (ctrl->flags & Flag_WidthFixed) {
                    base_width += ctrl->GetTakingUpWidth();
                }
                // 未指定宽度?
                else {
                    counter += 1.f;
                }
            }
        }
        // 校正
        base_width /= m_2fZoom.width;
        base_height /= m_2fZoom.height;
        // 计算
        base_height = std::max(base_height, this->view_size.height);
        // 宽度步进
        float width_step = counter > 0.f ? (this->view_size.width - base_width) / counter : 0.f;
        // 小于阈值
        if (width_step < float(LongUIAutoControlMinSize)) {
            width_step = float(LongUIAutoControlMinSize);
        }
        float position_x = 0.f;
        // 第二次
        for (auto ctrl : (*this)) {
            // 跳过浮动控件
            if (ctrl->flags & Flag_Floating) continue;
            // 设置控件高度
            if (!(ctrl->flags & Flag_HeightFixed)) {
                ctrl->SetHeight(base_height);
            }
            // 设置控件宽度
            if (!(ctrl->flags & Flag_WidthFixed)) {
                ctrl->SetWidth(width_step);
            }
            // 不管如何, 修改!
            ctrl->SetControlSizeChanged();
            ctrl->SetLeft(position_x);
            ctrl->SetTop(0.f);
            position_x += ctrl->GetTakingUpWidth();
        }
        // 修改
        m_2fContentSize.width = position_x;
        m_2fContentSize.height = base_height;
        // 已经处理
        this->ControlSizeChangeHandled();
    }
    // 父类刷新
    return Super::Update();
}

// UIHorizontalLayout 重建
auto LongUI::UIHorizontalLayout::Recreate(LongUIRenderTarget* newRT) noexcept ->HRESULT {
    auto hr = S_OK;
    if (newRT) {
        for (auto ctrl : (*this)) {
            hr = ctrl->Recreate(newRT);
            AssertHR(hr);
        }
    }
    return Super::Recreate(newRT);
}

// UIHorizontalLayout 关闭控件
void LongUI::UIHorizontalLayout::Cleanup() noexcept {
    delete this;
}

