import cloudinary.uploader
from config.setup import DEFAULT_CLOUDINARY_FOLDER

def upload_to_cloudinary(file_path, folder=DEFAULT_CLOUDINARY_FOLDER):
    try:
        response = cloudinary.uploader.upload(file_path, folder=folder)
        return response.get("secure_url")
    except Exception as e:
        print(f"Error: {e}")
        return None
