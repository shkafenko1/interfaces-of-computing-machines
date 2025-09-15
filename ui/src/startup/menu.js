document.addEventListener('DOMContentLoaded', () => {
  const cowImage = document.querySelector('.cowmenu');
  const menuButtons = document.querySelectorAll('.mc-button');

  const originalSrc = '../../public/cow.gif';
  const clickSrc = '../../public/clickcow.png';
  const buttonSound = '../../public/sounds/minecraft_click.mp3';
  const cowSound = '../../public/sounds/hurt3.wav';

  const moo = new Audio(cowSound);
  const click = new Audio(buttonSound);

  if (cowImage) {
    cowImage.addEventListener('click', () => {
      cowImage.src = clickSrc;
      moo.play().catch(error => console.error('Error playing sound:', error));

      setTimeout(() => {
        cowImage.src = originalSrc;
      }, 1000);
    });
  } else {
    console.error('The element with class "cowmenu" was not found.');
  }

  if (menuButtons.length > 0) {
    menuButtons.forEach(button => {
      button.addEventListener('click', (event) => {
        const link = button.parentElement;
        if (link.tagName === 'A' && link.href) {
          event.preventDefault();
          const audio = new Audio(buttonSound);
          audio.play();
          audio.addEventListener('ended', () => {
            window.location.href = link.href;
          });
        } else {
          new Audio(buttonSound).play();
        }
      });
    });
  }
}); 