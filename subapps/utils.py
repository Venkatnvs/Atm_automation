from datetime import datetime
import cloudinary.uploader
import pytz
from config.setup import DEFAULT_CLOUDINARY_FOLDER

def upload_to_cloudinary(file_path, folder=DEFAULT_CLOUDINARY_FOLDER):
    try:
        response = cloudinary.uploader.upload(file_path, folder=folder)
        return response.get("secure_url")
    except Exception as e:
        print(f"Error: {e}")
        return None

def format_datetime(value, format="%d/%m/%Y %I:%M:%S %p", tz="Asia/Kolkata"):
    dt = datetime.fromisoformat(value)
    if dt.tzinfo is None:
        dt = dt.replace(tzinfo=pytz.UTC)
    dt = dt.astimezone(pytz.timezone(tz))
    return dt.strftime(format)