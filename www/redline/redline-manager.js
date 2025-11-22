// 全局状态
let currentCamera = null;
let currentLineType = 'line';
let lines = [];
let drawingPoints = [];
let isDrawing = false;
let canvas, ctx, video;
let apiSecret = '035c73f7-bb6b-4889-a715-d9eb2d1925cc';

// 初始化
document.addEventListener('DOMContentLoaded', function() {
    canvas = document.getElementById('drawCanvas');
    ctx = canvas.getContext('2d');
    video = document.getElementById('videoPlayer');

    // 设置canvas事件
    canvas.addEventListener('mousedown', handleMouseDown);
    canvas.addEventListener('mousemove', handleMouseMove);
    canvas.addEventListener('mouseup', handleMouseUp);

    // 视频加载完成后调整canvas大小
    video.addEventListener('loadedmetadata', function() {
        resizeCanvas();
    });

    window.addEventListener('resize', resizeCanvas);

    // 加载所有摄像头配置
    loadAllCameras();

    // 设置secret
    const secretInput = document.getElementById('secretInput');
    secretInput.value = apiSecret;
    secretInput.addEventListener('change', function() {
        apiSecret = this.value;
    });
});

// 调整canvas大小以匹配视频
function resizeCanvas() {
    const rect = video.getBoundingClientRect();
    canvas.width = video.videoWidth || rect.width;
    canvas.height = video.videoHeight || rect.height;
    canvas.style.width = rect.width + 'px';
    canvas.style.height = rect.height + 'px';
    redrawLines();
}

// API调用函数
async function apiCall(endpoint, method = 'GET', data = null) {
    const url = `/index/api/${endpoint}${method === 'GET' && data ? '?' + new URLSearchParams(data) : ''}`;
    const options = {
        method: method,
        headers: {
            'Content-Type': 'application/json'
        }
    };

    if (method === 'POST' && data) {
        options.body = JSON.stringify(data);
    }

    try {
        const response = await fetch(url, options);
        const result = await response.json();

        if (result.code !== 0) {
            throw new Error(result.msg || '操作失败');
        }

        return result;
    } catch (error) {
        showAlert('错误: ' + error.message, 'error');
        throw error;
    }
}

// 加载所有摄像头配置
async function loadAllCameras() {
    try {
        const result = await apiCall('getAllRedLineConfigs', 'GET', { secret: apiSecret });
        const cameraListDiv = document.getElementById('cameraList');
        cameraListDiv.innerHTML = '';

        const cameras = result.data || {};
        const cameraIds = Object.keys(cameras);

        if (cameraIds.length === 0) {
            cameraListDiv.innerHTML = '<div class="help-text">暂无配置</div>';
            return;
        }

        cameraIds.forEach(cameraId => {
            const div = document.createElement('div');
            div.className = 'camera-item';
            div.textContent = cameraId;
            div.onclick = () => loadCamera(cameraId);
            cameraListDiv.appendChild(div);
        });
    } catch (error) {
        console.error('加载摄像头列表失败:', error);
    }
}

// 加载或创建摄像头配置
async function loadOrCreateCamera() {
    const cameraId = document.getElementById('cameraInput').value.trim();
    if (!cameraId) {
        showAlert('请输入摄像头ID', 'error');
        return;
    }

    loadCamera(cameraId);
}

// 加载摄像头配置
async function loadCamera(cameraId) {
    try {
        const result = await apiCall('getRedLineConfig', 'GET', {
            secret: apiSecret,
            camera_id: cameraId
        });

        currentCamera = cameraId;
        lines = result.data.lines || [];

        // 更新UI
        document.getElementById('currentCamera').textContent = cameraId;
        document.getElementById('cameraConfigSection').style.display = 'block';
        document.getElementById('cameraInput').value = cameraId;

        // 更新摄像头列表中的active状态
        document.querySelectorAll('.camera-item').forEach(item => {
            item.classList.toggle('active', item.textContent === cameraId);
        });

        // 重绘红线
        redrawLines();
        updateLineList();

        showAlert('成功加载摄像头配置: ' + cameraId, 'success');
    } catch (error) {
        console.error('加载摄像头配置失败:', error);
    }
}

// 加载视频
function loadVideo() {
    const url = document.getElementById('streamUrlInput').value.trim();
    if (!url) {
        showAlert('请输入视频流URL', 'error');
        return;
    }

    video.src = url;
    video.load();
    video.play().catch(error => {
        showAlert('视频加载失败: ' + error.message, 'error');
    });
}

// 选择线条类型
function selectLineType(type) {
    currentLineType = type;
    document.querySelectorAll('.type-btn').forEach(btn => {
        btn.classList.toggle('active', btn.dataset.type === type);
    });

    // 重置绘制状态
    drawingPoints = [];
    isDrawing = false;
    redrawLines();
}

// 鼠标事件处理
function handleMouseDown(e) {
    if (!currentCamera) {
        showAlert('请先选择摄像头', 'error');
        return;
    }

    const rect = canvas.getBoundingClientRect();
    const scaleX = canvas.width / rect.width;
    const scaleY = canvas.height / rect.height;
    const x = Math.round((e.clientX - rect.left) * scaleX);
    const y = Math.round((e.clientY - rect.top) * scaleY);

    isDrawing = true;
    drawingPoints.push({ x, y });

    if (currentLineType === 'line' && drawingPoints.length === 2) {
        finishDrawing();
    } else if (currentLineType === 'rect' && drawingPoints.length === 2) {
        finishDrawing();
    }

    redrawLines();
}

function handleMouseMove(e) {
    if (!isDrawing) return;

    const rect = canvas.getBoundingClientRect();
    const scaleX = canvas.width / rect.width;
    const scaleY = canvas.height / rect.height;
    const x = Math.round((e.clientX - rect.left) * scaleX);
    const y = Math.round((e.clientY - rect.top) * scaleY);

    // 清除并重绘
    redrawLines();

    // 绘制当前正在画的线
    ctx.strokeStyle = document.getElementById('colorPicker').value;
    ctx.lineWidth = parseInt(document.getElementById('thicknessInput').value);
    ctx.beginPath();

    if (currentLineType === 'line' && drawingPoints.length === 1) {
        ctx.moveTo(drawingPoints[0].x, drawingPoints[0].y);
        ctx.lineTo(x, y);
    } else if (currentLineType === 'rect' && drawingPoints.length === 1) {
        const w = x - drawingPoints[0].x;
        const h = y - drawingPoints[0].y;
        ctx.rect(drawingPoints[0].x, drawingPoints[0].y, w, h);
    } else if (currentLineType === 'polygon' && drawingPoints.length > 0) {
        ctx.moveTo(drawingPoints[0].x, drawingPoints[0].y);
        for (let i = 1; i < drawingPoints.length; i++) {
            ctx.lineTo(drawingPoints[i].x, drawingPoints[i].y);
        }
        ctx.lineTo(x, y);
    }

    ctx.stroke();
}

function handleMouseUp(e) {
    // 多边形需要双击或右键完成
}

// 完成绘制
function finishDrawing() {
    if (drawingPoints.length < 2) {
        showAlert('至少需要两个点', 'error');
        return;
    }

    const newLine = {
        id: 'line_' + Date.now(),
        type: currentLineType,
        points: drawingPoints.map(p => [p.x, p.y]),
        color: document.getElementById('colorPicker').value,
        thickness: parseInt(document.getElementById('thicknessInput').value),
        label: document.getElementById('labelInput').value
    };

    lines.push(newLine);
    drawingPoints = [];
    isDrawing = false;

    redrawLines();
    updateLineList();

    showAlert('红线添加成功', 'success');
}

// 重绘所有红线
function redrawLines() {
    ctx.clearRect(0, 0, canvas.width, canvas.height);

    lines.forEach(line => {
        ctx.strokeStyle = line.color;
        ctx.lineWidth = line.thickness;
        ctx.fillStyle = line.color;

        if (line.type === 'line') {
            ctx.beginPath();
            ctx.moveTo(line.points[0][0], line.points[0][1]);
            ctx.lineTo(line.points[1][0], line.points[1][1]);
            ctx.stroke();
        } else if (line.type === 'rect') {
            const x1 = line.points[0][0];
            const y1 = line.points[0][1];
            const x2 = line.points[1][0];
            const y2 = line.points[1][1];
            const w = x2 - x1;
            const h = y2 - y1;
            ctx.strokeRect(x1, y1, w, h);
        } else if (line.type === 'polygon') {
            ctx.beginPath();
            ctx.moveTo(line.points[0][0], line.points[0][1]);
            for (let i = 1; i < line.points.length; i++) {
                ctx.lineTo(line.points[i][0], line.points[i][1]);
            }
            ctx.closePath();
            ctx.stroke();
        }

        // 绘制标签
        if (line.label) {
            ctx.font = '16px Arial';
            ctx.fillText(line.label, line.points[0][0] + 5, line.points[0][1] - 10);
        }
    });

    // 绘制当前正在画的点
    drawingPoints.forEach((point, index) => {
        ctx.fillStyle = '#00FF00';
        ctx.beginPath();
        ctx.arc(point.x, point.y, 5, 0, 2 * Math.PI);
        ctx.fill();
    });
}

// 更新红线列表
function updateLineList() {
    const lineListDiv = document.getElementById('lineList');
    lineListDiv.innerHTML = '';

    if (lines.length === 0) {
        lineListDiv.innerHTML = '<div class="help-text">暂无红线配置</div>';
        return;
    }

    lines.forEach((line, index) => {
        const div = document.createElement('div');
        div.className = 'line-item';

        const typeNames = { line: '直线', rect: '矩形', polygon: '多边形' };

        div.innerHTML = `
            <div class="line-item-header">
                <strong>${line.label || '红线 ' + (index + 1)}</strong>
                <button class="btn btn-danger" onclick="deleteLine(${index})">删除</button>
            </div>
            <div class="line-item-details">
                类型: ${typeNames[line.type]} |
                颜色: <span style="display:inline-block;width:20px;height:20px;background:${line.color};border:1px solid #ddd;vertical-align:middle;"></span> |
                粗细: ${line.thickness}px |
                点数: ${line.points.length}
            </div>
        `;

        lineListDiv.appendChild(div);
    });
}

// 删除红线
function deleteLine(index) {
    if (confirm('确定要删除这条红线吗?')) {
        lines.splice(index, 1);
        redrawLines();
        updateLineList();
    }
}

// 清空所有红线
function clearLines() {
    if (confirm('确定要清空所有红线吗?')) {
        lines = [];
        redrawLines();
        updateLineList();
    }
}

// 保存配置
async function saveConfig() {
    if (!currentCamera) {
        showAlert('请先选择摄像头', 'error');
        return;
    }

    try {
        const config = {
            camera_id: currentCamera,
            enabled: true,
            lines: lines
        };

        await apiCall('setRedLineConfig', 'POST', {
            secret: apiSecret,
            camera_id: currentCamera,
            ...config
        });

        showAlert('配置保存成功!', 'success');
        loadAllCameras(); // 刷新摄像头列表
    } catch (error) {
        console.error('保存配置失败:', error);
    }
}

// 显示提示
function showAlert(message, type) {
    const alertContainer = document.getElementById('alertContainer');
    const alertDiv = document.createElement('div');
    alertDiv.className = `alert alert-${type}`;
    alertDiv.textContent = message;

    alertContainer.innerHTML = '';
    alertContainer.appendChild(alertDiv);

    setTimeout(() => {
        alertDiv.remove();
    }, 3000);
}

// 双击完成多边形绘制
canvas.addEventListener('dblclick', function(e) {
    if (currentLineType === 'polygon' && drawingPoints.length >= 3) {
        finishDrawing();
    }
});

// 右键取消绘制
canvas.addEventListener('contextmenu', function(e) {
    e.preventDefault();
    drawingPoints = [];
    isDrawing = false;
    redrawLines();
    return false;
});
