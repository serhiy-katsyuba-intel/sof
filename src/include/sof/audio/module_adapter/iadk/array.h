#ifndef SOF_AUDIO_MODULE_ADAPTER_IADK_ARRAY_H
#define SOF_AUDIO_MODULE_ADAPTER_IADK_ARRAY_H

#include <stddef.h>
#include <stdint.h>

namespace intel_adsp
{
	template<class Value>
	struct Array {
		typedef Value ValueType;

		explicit Array() : array_(NULL), length_(0) {}
		Array(Value *array, size_t length) : array_(array), length_(length) {}

		bool IsInitialized() const
		{
			return array_ != NULL;
		}

		void Init(Value *array, size_t length)
		{
			array_ = array;
			length_ = length;
		}

		Value GetValue(int index = 0) const
		{
			return array_[index];
		}

		Value operator[](int index) const
		{
			return GetValue(index);
		}

		size_t size() const
		{
			return length_;
		}

		const Value *data() const
		{
			return array_;
		}

		Value *data()
		{
			return array_;
		}

		size_t GetLength() const
		{
			return length_;
		}

		void Copy(Value *array, size_t length) const
		{
			if (!array_)
				return;

			if (length > length_)
				length = length_;

			for (size_t index = 0; index < length; ++index)
				array[index] = array_[index];
		}

	private:
		Value *array_;
		size_t length_;
	};

	typedef Array<uint8_t> IAByteArray;
}

#endif /* SOF_AUDIO_MODULE_ADAPTER_IADK_ARRAY_H */