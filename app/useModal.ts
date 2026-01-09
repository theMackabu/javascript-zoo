import { onMounted, onUnmounted } from 'vue';

type ModalOptions = {
  onClose: () => void;
  bodyClass?: string;
};

export function useModal(options: ModalOptions): void {
  const { onClose, bodyClass } = options;

  function onKeydown(event: KeyboardEvent) {
    if (event.key === 'Escape') {
      onClose();
    }
  }

  onMounted(() => {
    window.addEventListener('keydown', onKeydown);
    if (bodyClass) {
      document.body.classList.add(bodyClass);
    }
  });

  onUnmounted(() => {
    window.removeEventListener('keydown', onKeydown);
    if (bodyClass) {
      document.body.classList.remove(bodyClass);
    }
  });
}
