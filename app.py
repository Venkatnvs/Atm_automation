from io import BytesIO
import os
from PIL import Image
from flask import Flask, render_template, request, redirect, url_for, jsonify
from flask_socketio import SocketIO, emit
from config.setup import initialize_firebase, initialize_cloudinary
from subapps.face import add_new_face, process_image, remove_user_encodings
import threading
from subapps.utils import upload_to_cloudinary, format_datetime
import cv2
import numpy as np
from datetime import datetime
import random
import string
import smtplib
from email.mime.text import MIMEText
from email.mime.multipart import MIMEMultipart

initialize_cloudinary()
db = initialize_firebase()

app = Flask(__name__, template_folder="templates")
app.jinja_env.filters['format_datetime'] = format_datetime
socketio = SocketIO(app, cors_allowed_origins="*")

if not os.path.exists("uploads"):
    os.makedirs("uploads")

otp_store = {}
current_verification = None

def generate_otp():
    return ''.join(random.choices(string.digits, k=6))

def send_otp_email(email, otp):
    sender_email = os.environ.get("EMAIL_HOST_USER")
    sender_password = os.environ.get("EMAIL_HOST_PASSWORD")
    
    msg = MIMEMultipart()
    msg['From'] = sender_email
    msg['To'] = email
    msg['Subject'] = "Your OTP for Face Recognition"
    
    body = f"Your OTP is: {otp}. This OTP will expire in 2 minutes."
    msg.attach(MIMEText(body, 'plain'))
    try:
        server = smtplib.SMTP('smtp.gmail.com', 587)
        server.starttls()
        server.login(sender_email, sender_password)
        server.send_message(msg)
        server.quit()
        return True
    except Exception as e:
        print(f"Error sending email: {e}")
        return False

@app.route('/')
def index():
    ref = db.reference("users")
    users = ref.get() or {}
    return render_template("index.html", users=users)

@app.route('/add_user', methods=['POST'])
def add_user():
    data = request.form
    name, email, uuid = data["name"], data["email"], data["uuid"]

    # Upload face image
    face_img = request.files["face_img"]
    face_url = upload_to_cloudinary(face_img)

    # Save user to Firebase
    db.reference(f"users/{uuid}").set({
        "name": name,
        "email": email,
        "face_url": face_url,
        "isAddedToMLModel": False,
        "uuid": uuid,
        "faces": {}
    })
    threading.Thread(target=add_new_face, args=(uuid,), daemon=True).start()
    return redirect(url_for("index"))

@app.route('/user/<user_id>')
def user_details(user_id):
    ref = db.reference(f"users/{user_id}/images")
    images = ref.get() or {}
    all_images = []
    for image in images.values():
        all_images.append({
            "image_url": image["image_url"],
            "timestamp": image["timestamp"],
            "is_verified": image["is_verified"] if "is_verified" in image else False
        })
    all_images.sort(key=lambda x: x["timestamp"], reverse=True)
    return render_template("user.html", user_id=user_id, images=all_images)

@socketio.on('connect')
def handle_connect():
    print('Client connected')
    if current_verification:
        emit('otp_status', {'verification': current_verification})

@socketio.on('disconnect')
def handle_disconnect():
    print('Client disconnected')

@socketio.on('check_otp_status')
def handle_check_otp_status():
    global current_verification
    if current_verification:
        current_time = datetime.now().timestamp()
        if current_time - current_verification['timestamp'] > 120:
            current_verification = None
            emit('otp_status', {'verification': None})
        else:
            emit('otp_status', {'verification': current_verification})
    else:
        emit('otp_status', {'verification': None})

@app.route('/face_recognition', methods=['POST'])
def face_recognition():
    global current_verification
    if current_verification and current_verification.get('timestamp') and datetime.now().timestamp() - current_verification.get('timestamp') < 120:
        return jsonify({
            'status': 'error',
            'message': 'Another verification is in progress'
        }), 400
    file = request.data
    img_buffer = BytesIO(file)
    img = Image.open(img_buffer)
    img = img.rotate(180)
    rotated_buffer = BytesIO()
    img.save(rotated_buffer, format="JPEG")
    rotated_buffer.seek(0)
    image_path = f"uploads/temp.jpg"
    with open(image_path, "wb") as f:
        f.write(rotated_buffer.getvalue())
    image = cv2.imdecode(np.frombuffer(rotated_buffer.getvalue(), np.uint8), cv2.IMREAD_COLOR)
    if image is None:
        return jsonify({
            'status': 'error',
            'message': 'Invalid image format'
        }), 400
    is_success, buffer = cv2.imencode('.jpg', image)
    if not is_success:
        return jsonify({
            'status': 'error',
            'message': 'Error processing image'
        }), 400        
    result = process_image(image)
    current_user_id = str(db.reference(f"esp/current_id").get())
    face_url = upload_to_cloudinary(image_path)
    
    db.reference(f"users/{current_user_id}/images/").push({
        "image_url": face_url,
        "timestamp": datetime.now().isoformat(),
        "is_verified": True if result == current_user_id else False
    })
    
    if result and result != "Unknown":
        user_id = current_user_id
        user_data = db.reference(f"users/{user_id}").get()
        if user_data and 'email' in user_data:
            email = user_data['email']
            otp = generate_otp()
            otp_store[user_id] = {
                'otp': otp,
                'timestamp': datetime.now().timestamp()
            }
            current_verification = {
                'user_id': user_id,
                'email': email,
                'name': user_data.get('name', 'User'),
                'timestamp': datetime.now().timestamp()
            }
            send_otp_email(email, otp)
            socketio.emit('otp_status', {'verification': current_verification})
            db.reference(f"esp/triggers/action").set("success_rec")
        return jsonify({
            'status': 'success',
            'user_id': user_id,
            'message': 'Face recognized, OTP sent'
        })
    else:
        print("Face verification failed")
        db.reference(f"esp/triggers/action").set("failed_rec")
        socketio.emit('face_recognition_failed', {
            'image_url': face_url
        })
        return jsonify({
            'status': 'error',
            'message': 'Face verification failed',
            'image_url': face_url
        }), 404

@app.route('/verify_otp', methods=['POST'])
def verify_otp():
    global current_verification
    data = request.json
    user_id = data.get('user_id')
    otp = data.get('otp')
    
    if not current_verification or current_verification['user_id'] != user_id:
        return jsonify({
            'status': 'error',
            'message': 'No active verification found'
        }), 400
    
    if user_id in otp_store:
        stored_data = otp_store[user_id]
        if datetime.now().timestamp() - stored_data['timestamp'] > 120:
            del otp_store[user_id]
            current_verification = None
            socketio.emit('otp_status', {'verification': None})
            return jsonify({
                'status': 'error',
                'message': 'OTP expired'
            }), 400
        if stored_data['otp'] == otp:
            del otp_store[user_id]
            current_verification = None
            db.reference(f"esp/triggers/action").set("open_door")
            socketio.emit('otp_status', {'verification': None})
            return jsonify({
                'status': 'success',
                'message': 'OTP verified successfully'
            })
    return jsonify({
        'status': 'error',
        'message': 'Invalid OTP'
    }), 400

@app.route('/delete_user/<user_id>', methods=['POST'])
def delete_user(user_id):
    try:
        db.reference(f"users/{user_id}").delete()
        remove_user_encodings(user_id)
        return jsonify({
            'status': 'success',
            'message': 'User deleted successfully'
        })
    except Exception as e:
        return jsonify({
            'status': 'error',
            'message': f'Error deleting user: {str(e)}'
        }), 500

if __name__ == '__main__':
    socketio.run(app, debug=True, host="0.0.0.0", port=5000)
